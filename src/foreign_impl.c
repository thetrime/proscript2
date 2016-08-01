#include "kernel.h"
#include "errors.h"
#include "stream.h"
#include "parser.h"
#include "options.h"
#include "operators.h"
#include "prolog_flag.h"
#include "arithmetic.h"
#include "char_conversion.h"
#include <stdio.h>

// 8.2.1
PREDICATE(=, 2, (word a, word b)
{
   return unify(a,b);
})

// 8.2.2
PREDICATE(unify_with_occurs_check, 2, (word a, word b)
{
   return unify(a, b) && acyclic_term(a);
})

// 8.2.3 \= is always compiled


// 8.3.1
PREDICATE(var, 1, (word a)
{
   return TAGOF(a) == VARIABLE_TAG;
})

// 8.3.2
PREDICATE(atom, 1, (word a)
{
   return TAGOF(a) == CONSTANT_TAG && getConstant(a).type == ATOM_TYPE;
})

// 8.3.3
PREDICATE(integer, 1, (word a)
{
   return TAGOF(a) == CONSTANT_TAG && getConstant(a).type == INTEGER_TYPE;
})

// 8.3.4
PREDICATE(float, 1, (word a)
{
   return TAGOF(a) == CONSTANT_TAG && getConstant(a).type == FLOAT_TYPE;
})

// 8.3.5
PREDICATE(atomic, 1, (word a)
{
   return TAGOF(a) == CONSTANT_TAG;
})

// 8.3.6
PREDICATE(compound, 1, (word a)
{
   return TAGOF(a) == COMPOUND_TAG;
})

// 8.3.7
PREDICATE(nonvar, 1, (word a)
{
   return TAGOF(a) != VARIABLE_TAG;
})

// 8.3.8
PREDICATE(number, 1, (word a)
{
   constant c;
   if (TAGOF(a) == CONSTANT_TAG)
   {
      c = getConstant(a);
      return (c.type == INTEGER_TYPE || c.type == FLOAT_TYPE || c.type == BIGINTEGER_TYPE || c.type == RATIONAL_TYPE);
   }
   return 0;
})

// 8.4.1
PREDICATE(@=<, 2, (word a, word b)
{
   return term_difference(a, b) <= 0;
})

PREDICATE(==, 2, (word a, word b)
{
   return term_difference(a, b) == 0;
})

PREDICATE(\\==, 2, (word a, word b)
{
   return term_difference(a, b) != 0;
})

PREDICATE(@<, 2, (word a, word b)
{
   return term_difference(a, b) < 0;
})

PREDICATE(@>, 2, (word a, word b)
{
   return term_difference(a, b) > 0;
})

PREDICATE(@>=, 2, (word a, word b)
{
   return term_difference(a, b) >= 0;
})

// 8.5.1
PREDICATE(functor, 3, (word term, word name, word arity)
{
   if (TAGOF(term) == VARIABLE_TAG)
   {
      if (!(must_be_positive_integer(arity) &&
            must_be_bound(name) &&
            must_be_atom(name)))
         return 0;
      long a = getConstant(arity).data.integer_data->data;
      if (a == 0)
         return unify(term, name);
      word* args = malloc(sizeof(word) * a);
      for (int i = 0; i < a; i++)
         args[i] = MAKE_VAR();
      word w = MAKE_ACOMPOUND(MAKE_FUNCTOR(name, a), args);
      free(args);
      return unify(term, w);
   }
   else if (TAGOF(term) == CONSTANT_TAG)
      return unify(name, term) && unify(arity, MAKE_INTEGER(0));
   else if (TAGOF(term) == COMPOUND_TAG)
   {
      Functor f = getConstant(FUNCTOROF(term)).data.functor_data;
      return unify(name, f->name) && unify(arity, MAKE_INTEGER(f->arity));
   }
   return type_error(compoundAtom, term);
})

// 8.5.2
NONDET_PREDICATE(arg, 3, (word n, word term, word arg, word backtrack)
{
   if (!must_be_bound(term))
      return 0;
   if (TAGOF(term) != COMPOUND_TAG)
      type_error(compoundAtom, term);
   if (TAGOF(n) == VARIABLE_TAG)
   {
      // -,+,? mode
      Functor functor = getConstant(FUNCTOROF(term)).data.functor_data;
      long index = backtrack == 0?0:getConstant(backtrack).data.integer_data->data;
      if (index + 1 < functor->arity)
         make_foreign_choicepoint(MAKE_INTEGER(index+1));
      if (index >= functor->arity)
         return 0;
      return (unify(n, MAKE_INTEGER(index+1)) && // arg is 1-based, but all our terms are 0-based
              unify(ARGOF(term, index), arg));
   }
   if (!must_be_positive_integer(n))
      return 0;
   long _n = getConstant(n).data.integer_data->data;
   Functor functor = getConstant(FUNCTOROF(term)).data.functor_data;
   if (_n > functor->arity)
      return FAIL; // N is too big
   else if (_n < 1)
      return FAIL; // N is too small
   return unify(ARGOF(term, _n-1), arg);

})

// 8.5.3
PREDICATE(=.., 2, (word term, word univ)
{
   if (TAGOF(term) == CONSTANT_TAG)
   {
      List list;
      word w;
      init_list(&list);
      list_append(&list, term);
      w = term_from_list(&list, emptyListAtom);
      free_list(&list);
      return unify(univ, w);
   }
   else if (TAGOF(term) == COMPOUND_TAG)
   {
      List list;
      word w;
      init_list(&list);
      Functor f = getConstant(FUNCTOROF(term)).data.functor_data;
      list_append(&list, f->name);
      for (int i = 0; i < f->arity; i++)
         list_append(&list, ARGOF(term, i));
      w = term_from_list(&list, emptyListAtom);
      free_list(&list);
      return unify(univ, w);
   }
   else if (TAGOF(term) == VARIABLE_TAG)
   {
      List list;
      init_list(&list);
      if (!populate_list_from_term(&list, univ))
      {
         free_list(&list);
         return 0; // Error
      }
      word fname = list_shift(&list);
      if (!must_be_bound(fname))
      {
         free_list(&list);
         return 0; // Error
      }
      int rc = unify(term, MAKE_LCOMPOUND(fname, &list));
      free_list(&list);
      return rc;
   }
   return type_error(compoundAtom, term);
})

// 8.5.4
PREDICATE(copy_term, 2, (word term, word copy)
{
   return unify(copy, copy_term(term));
})

// 8.6.1
PREDICATE(is, 2, (word result, word expr)
{
   assert(0 && "Not implenented");
})

// 8.7.1 Arithmetic comparisons
PREDICATE(=\\=, 2, (word a, word b)
{
   return arith_compare(a, b) != 0;
})

PREDICATE(=:=, 2, (word a, word b)
{
   return arith_compare(a, b) == 0;
})

PREDICATE(>, 2, (word a, word b)
{
   return arith_compare(a, b) > 0;
})

PREDICATE(>=, 2, (word a, word b)
{
   return arith_compare(a, b) >= 0;
})

PREDICATE(=<, 2, (word a, word b)
{
   return arith_compare(a, b) <= 0;
})

PREDICATE(<, 2, (word a, word b)
{
   return arith_compare(a, b) < 0;
})

// 8.8.1
NONDET_PREDICATE(clause, 2, (word head, word body, word backtrack)
{
   if (TAGOF(body) == CONSTANT_TAG)
   {
      constant c = getConstant(body);
      if (c.type == INTEGER_TYPE || c.type == FLOAT_TYPE)
         type_error(callableAtom, body);
   }
   Module module;
   word functor;
   if (!head_functor(head, &module, &functor))
   {
      return 0; // Error
   }
   Predicate p = lookup_predicate(module, functor);
   if (p == NULL)
      return FAIL;
   if (p->flags && PREDICATE_FOREIGN)
      permission_error(accessAtom, privateProcedureAtom, predicate_indicator(head));
   predicate_cell_t* c = backtrack == 0?p->head:GET_POINTER(backtrack);
   if (c == NULL)
      return FAIL;
   if (c->next != NULL)
      make_foreign_choicepoint(MAKE_POINTER(c->next));
   word clause = c->term;
   if (TAGOF(clause) == COMPOUND_TAG)
   {
      return unify(ARGOF(clause,0), head) && unify(ARGOF(clause,1), body);
   }
   else
   {
      return unify(clause, head) && unify(trueAtom, body);
   }

})

// 8.8.2
NONDET_PREDICATE(current_predicate, 1, (word indicator, word backtrack)
{
   if (TAGOF(indicator) != VARIABLE_TAG)
   {
      if ((TAGOF(indicator) != COMPOUND_TAG && FUNCTOROF(indicator) == predicateIndicatorFunctor))
         type_error(predicateIndicatorAtom, indicator);
      if ((TAGOF(ARGOF(indicator, 0)) != VARIABLE_TAG) && !(TAGOF(ARGOF(indicator, 0)) == CONSTANT_TAG && getConstant(ARGOF(indicator, 0)).type != ATOM_TYPE))
         type_error(predicateIndicatorAtom, indicator);
      if ((TAGOF(ARGOF(indicator, 1)) != VARIABLE_TAG) && !(TAGOF(ARGOF(indicator, 1)) == CONSTANT_TAG && getConstant(ARGOF(indicator, 1)).type != INTEGER_TYPE))
         type_error(predicateIndicatorAtom, indicator);
   }
   // Now indicator is either unbound or bound to /(A,B)
   // FIXME: assumes current module
   word predicates;
   if (backtrack == 0)
   {
      List list;
      init_list(&list);
      whashmap_iterate(get_current_module()->predicates, build_predicate_list, &list);
      predicates = term_from_list(&list, emptyListAtom);
      free_list(&list);
   }
   else
      predicates = backtrack;
   if (ARGOF(predicates, 1) != emptyListAtom)
      make_foreign_choicepoint(ARGOF(predicates, 1));
   return unify(indicator, predicate_indicator(ARGOF(predicates, 0)));

})

// 8.9.1
PREDICATE(asserta, 1, (word term)
{
   assert(0);
})

// 8.9.2
PREDICATE(assertz, 1, (word term)
{
   assert(0);
})

// 8.9.3
NONDET_PREDICATE(rtract, 1, (word term, word backtrack)
{
   assert(0);
})

// 8.9.4
PREDICATE(abolish, 1, (word indicator)
{
   assert(0);
})

// 8.10 (findall/3, bagof/3, setof/3) are in builtin.pl (note that they might be much faster if implemented directly in C)
// 8.11.1
PREDICATE(current_input, 1, (word stream)
{
   return unify(stream, current_input->term);
})

// 8.11.2
PREDICATE(current_output, 1, (word stream)
{
   return unify(stream, current_output->term);
})

// 8.11.3
PREDICATE(set_input, 1, (word stream)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   current_input = s;
   return SUCCESS;
})

// 8.11.4
PREDICATE(set_output, 1, (word stream)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   current_output = s;
   return SUCCESS;
})
// 8.11.5 open/3,4
PREDICATE(open, 3, (word source_sink, word io_mode, word stream)
{
   if (!must_be_atom(source_sink))
      return ERROR;
   Options options;
   init_options(&options);
   Stream s = fileStream(getConstant(source_sink).data.atom_data->data, io_mode, &options);
   free_options(&options);
   if (s == NULL)
      return ERROR;
   return unify(stream, s->term);
})
PREDICATE(open, 4, (word source_sink, word io_mode, word stream, word options)
{
   if (!must_be_atom(source_sink))
      return ERROR;
   Options _options;
   init_options(&_options);
   options_from_term(&_options, options);
   Stream s = fileStream(getConstant(source_sink).data.atom_data->data, io_mode, &_options);
   free_options(&_options);
   if (s == NULL)
      return ERROR;
   return unify(stream, s->term);
})

// 8.11.6 close/1,2
PREDICATE(close, 1, (word stream)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   if (s->close != NULL)
      return s->close(s) >= 0;
   return SUCCESS;
})

PREDICATE(close, 1, (word stream, word options)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   Options _options;
   init_options(&_options);
   options_from_term(&_options, options);
   int rc = 0;
   if (s->close != NULL)
      rc = s->close(s) >= 0;
   else
      rc = SUCCESS;
   free_options(&_options);
   return SUCCESS;
})

// 8.11.7 flush_output/0, 1
PREDICATE(flush_output, 0, ()
{
   if (current_output->flush != NULL)
      return current_output->flush(current_output) >= 0;
   return SUCCESS;
})
PREDICATE(flush_output, 1, (word stream)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   if (s->flush != NULL)
      return s->flush(s) >= 0;
   return SUCCESS;
})

// 8.11.8 stream_property/2, at_end_of_stream/0,1
NONDET_PREDICATE(stream_property, 2, (word stream, word property, word backtrack)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   long index = backtrack==0?0:getConstant(backtrack).data.integer_data->data;
   if (stream_properties[index] == NULL)
      return FAIL;
   if (stream_properties[index + 1] != NULL)
      make_foreign_choicepoint(MAKE_INTEGER(index+1));
   return stream_properties[index](s, property);
})
PREDICATE(at_end_of_stream, 0, ()
{
   return peekch(current_input) != -1;
})

PREDICATE(at_end_of_stream, 1, (word stream)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   return peekch(s) != -1;
})


// 8.11.9 set_stream_position/2
PREDICATE(set_stream_position, 2, (word stream, word position)
{
   if (!must_be_integer(position))
      return ERROR;
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   if (s->seek == NULL)
      return FAIL;
   return s->seek(s, (int)getConstant(position).data.integer_data, SEEK_SET) >= 0;
})

// 8.12.1 get_char/1,2 get_code/1,2
PREDICATE(get_char, 1, (word c)
{
   char ch = getch(current_input);
   return unify(c, MAKE_NATOM(&ch, 1));
})
PREDICATE(get_char, 2, (word stream, word c)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   char ch = getch(s);
   return unify(c, MAKE_NATOM(&ch, 1));
})
PREDICATE(get_code, 1, (word code)
{
   return unify(code, MAKE_INTEGER(getch(current_input)));
})
PREDICATE(get_code, 2, (word stream, word code)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   return unify(code, MAKE_INTEGER(getch(s)));
})

// 8.12.2 peek_char/1,2, peek_code/1,2
PREDICATE(peek_char, 1, (word c)
{
   char ch = peekch(current_input);
   return unify(c, MAKE_NATOM(&ch, 1));
})
PREDICATE(peek_char, 2, (word stream, word c)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   char ch = peekch(s);
   return unify(c, MAKE_NATOM(&ch, 1));
})
PREDICATE(peek_code, 1, (word code)
{
   return unify(code, MAKE_INTEGER(peekch(current_input)));
})
PREDICATE(peek_code, 2, (word stream, word code)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   return unify(code, MAKE_INTEGER(peekch(s)));
})

// 8.12.3 put_char/1,2, put_code/1,2
PREDICATE(put_char, 1, (word c)
{
   if (!must_be_character(c)) return ERROR;
   putch(current_input, getConstant(c).data.atom_data->data[0]);
   return SUCCESS;
})
PREDICATE(put_char, 2, (word stream, word c)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   if (!must_be_character(c)) return ERROR;
   putch(s, getConstant(c).data.atom_data->data[0]);
   return SUCCESS;
})
PREDICATE(put_code, 1, (word code)
{
   if (!must_be_positive_integer(code)) return ERROR;
   putch(current_input, (int)getConstant(code).data.integer_data->data);
   return SUCCESS;
})
PREDICATE(put_code, 2, (word stream, word code)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   if (!must_be_positive_integer(code)) return ERROR;
   putch(s, getConstant(code).data.integer_data->data);
   return SUCCESS;
})

// 8.13.1 get_byte/1,2
PREDICATE(get_byte, 1, (word c)
{
   int b = getb(current_input);
   return unify(c, MAKE_INTEGER(b));
})
PREDICATE(get_byte, 2, (word stream, word c)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   int b = getb(s);
   return unify(c, MAKE_INTEGER(b));
})
// 8.13.2 peek_byte/1,2
PREDICATE(peek_byte, 1, (word c)
{
   int b = peekb(current_input);
   return unify(c, MAKE_INTEGER(b));
})
PREDICATE(peek_byte, 2, (word stream, word c)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   int b = peekb(s);
   return unify(c, MAKE_INTEGER(b));
})

// 8.13.3 put_byte/1,2
PREDICATE(put_byte, 1, (word c)
{
   if (!must_be_positive_integer(c)) return ERROR;
   putb(current_input, (int)getConstant(c).data.integer_data->data);
   return SUCCESS;
})
PREDICATE(put_byte, 2, (word stream, word c)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   if (!must_be_positive_integer(c)) return ERROR;
   putb(s, getConstant(c).data.integer_data->data);
   return SUCCESS;
})

// 8.14.1 read_term/2,3 read/1,2
PREDICATE(read_term, 2, (word term, word options)
{
   Options _options;
   init_options(&_options);
   options_from_term(&_options, options);
   word t = read_term(current_input, &_options);
   free_options(&_options);
   return unify(term, t);
})
PREDICATE(read_term, 3, (word stream, word term, word options)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   Options _options;
   init_options(&_options);
   options_from_term(&_options, options);
   word t = read_term(current_input, &_options);
   free_options(&_options);
   return unify(term, t);
})
PREDICATE(read, 1, (word term)
{
   Options _options;
   init_options(&_options);
   word t = read_term(current_input, &_options);
   free_options(&_options);
   return unify(term, t);
})
PREDICATE(read, 2, (word stream, word term)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   Options _options;
   init_options(&_options);
   word t = read_term(current_input, &_options);
   free_options(&_options);
   return unify(term, t);
})

// 8.14.2 write_term/2,3 write/1,2, writeq/1,2, write_canonical/1,2 IMPLEMENT
// 8.14.3 op/3
PREDICATE(op, 3, (word priority, word fixity, word op)
{
   // FIXME: Should support a list as op, apparently
   if (!must_be_atom(fixity)) return ERROR;
   if (!must_be_atom(op)) return ERROR;
   if (!must_be_integer(priority)) return ERROR;
   Atom op_c = getConstant(op).data.atom_data;
   int pri = (int)getConstant(priority).data.integer_data->data;
   if (fixity == FXAtom)
   {
      add_operator(op_c->data, pri, FX);
      return SUCCESS;
   }
   else if (fixity == FYAtom)
   {
      add_operator(op_c->data, pri, FY);
      return SUCCESS;
   }
   else if (fixity == XFXAtom)
   {
      add_operator(op_c->data, pri, XFY);
      return SUCCESS;
   }
   else if (fixity == XFYAtom)
   {
      add_operator(op_c->data, pri, XFY);
      return SUCCESS;
   }
   else if (fixity == YFXAtom)
   {
      add_operator(op_c->data, pri, YFX);
      return SUCCESS;
   }
   else if (fixity == XFAtom)
   {
      add_operator(op_c->data, pri, XF);
      return SUCCESS;
   }
   else if (fixity == YFAtom)
   {
      add_operator(op_c->data, pri, YF);
      return SUCCESS;
   }
   return domain_error(operatorSpecifierAtom, fixity);
})

// 8.14.4 current_op/3
NONDET_PREDICATE(current_op, 3, (word priority, word fixity, word op, word backtrack)
{
   word list = (backtrack == 0?make_op_list():backtrack);
   if (ARGOF(list, 1) != emptyListAtom)
      make_foreign_choicepoint(ARGOF(list, 1));
   word head = ARGOF(list, 0);
   // head is for the form op(priority, fixity, op)
   return unify(priority, ARGOF(head,0))
      &&  unify(fixity, ARGOF(head, 1))
      &&  unify(op, ARGOF(head, 2));
})

// 8.14.5 char_conversion/2
PREDICATE(char_conversion, 2, (word in_char, word out_char)
{
   if (!must_be_character(in_char)) return ERROR;
   if (!must_be_character(out_char)) return ERROR;
   CharConversionTable[getConstant(in_char).data.atom_data->data[0]] = getConstant(out_char).data.atom_data->data[0];
   return SUCCESS;
})
// 8.14.6 current_char_conversion/2
NONDET_PREDICATE(current_char_conversion, 2, (word in_char, word out_char, word backtrack)
{
   if (TAGOF(in_char) == CONSTANT_TAG)
   {
      constant ci = getConstant(in_char);
      if (ci.type == ATOM_TYPE)
         return unify(out_char, MAKE_NATOM(&ci.data.atom_data->data[0], 1));
      return type_error(characterAtom, in_char);
   }
   else if (TAGOF(in_char) == VARIABLE_TAG)
   {
      if (TAGOF(out_char) == CONSTANT_TAG)
      {
         constant co = getConstant(in_char);
         if (co.type == ATOM_TYPE)
         {
            char needle = co.data.atom_data->data[0];
            // We have to search the table to see if something matches
            int startindex = 0;
            if (backtrack != 0)
               startindex = (int)getConstant(backtrack).data.integer_data->data;
            for (int i = startindex; i < 256; i++)
            {
               if (CharConversionTable[i] == needle)
               {
                  if (i < 256)
                     make_foreign_choicepoint(MAKE_INTEGER(i+1));
                  return unify(in_char, MAKE_INTEGER(i));
               }
            }
            return FAIL;
         }
         else
            return type_error(characterAtom, in_char);
      }
      else if (TAGOF(out_char) == VARIABLE_TAG)
      {
         // We must enumerate the entire table
         int index = 0;
         if (backtrack != 0)
            index = (int)getConstant(backtrack).data.integer_data->data;
         if (index < 256)
            make_foreign_choicepoint(MAKE_INTEGER(index+1));
         char i = (char)index;
         char o = CharConversionTable[i];
         return unify(in_char, MAKE_NATOM(&i, 1)) && unify(out_char, MAKE_NATOM(&o, 1));
      }
   }
   return type_error(characterAtom, in_char);
})

// 8.15.1 \+ is always compiled to opcodes
// 8.15.2 once/1 is in builtin.pl

// 8.15.3
NONDET_PREDICATE(repeat, 0, (word backtrack)
{
   make_foreign_choicepoint(0);
   return SUCCESS;
})

// 8.16.1
PREDICATE(atom_length, 2, (word atom, word length)
{
   if (TAGOF(atom) == VARIABLE_TAG)
      return instantiation_error();
   if (TAGOF(atom) != CONSTANT_TAG)
      type_error(atomAtom, atom);
   constant c = getConstant(atom);
   if (c.type != ATOM_TYPE)
      type_error(atomAtom, atom);
   if (TAGOF(length) != VARIABLE_TAG && !(must_be_integer(length)))
      return ERROR;
   if (TAGOF(length) == CONSTANT_TAG && !(must_be_positive_integer(length)))
      return ERROR;
   return unify(length, MAKE_INTEGER(c.data.atom_data->length));
})

// 8.1.6.2
NONDET_PREDICATE(atom_concat, 3, (word atom1, word atom2, word atom12, word backtrack)
{
   long index = 0;
   if (backtrack == 0)
   { // First call
      if (TAGOF(atom1) == VARIABLE_TAG && TAGOF(atom12) == VARIABLE_TAG)
         instantiation_error();
      if (TAGOF(atom2) == VARIABLE_TAG && TAGOF(atom12) == VARIABLE_TAG)
         instantiation_error();
      if (TAGOF(atom1) != VARIABLE_TAG)
         if (!must_be_atom(atom1))
            return ERROR;
      if (TAGOF(atom2) != VARIABLE_TAG)
         if (!must_be_atom(atom2))
            return ERROR;
      if (TAGOF(atom12) != VARIABLE_TAG)
         if (!must_be_atom(atom12))
            return ERROR;
      if (TAGOF(atom1) == CONSTANT_TAG  && TAGOF(atom2) == CONSTANT_TAG)
      {
         // Deterministic case
         Atom a1 = getConstant(atom1).data.atom_data;
         Atom a2 = getConstant(atom2).data.atom_data;
         char* new_atom = malloc(a1->length + a2->length);
         memcpy(new_atom, a1->data, a1->length);
         memcpy(new_atom+a1->length, a2->data, a2->length);
         word n = MAKE_NATOM(new_atom, a1->length + a2->length);
         free(new_atom);
         return unify(atom12, n);
      }
      // Non-deterministic case
      index = 0;
   }
   else
      index = getConstant(backtrack).data.integer_data->data;
   Atom a12 = getConstant(atom12).data.atom_data;
   if (index == a12->length+1)
      return FAIL;
   make_foreign_choicepoint(MAKE_INTEGER(index+1));
   return unify(atom1, MAKE_NATOM(a12->data, index)) && unify(atom2, MAKE_NATOM(a12->data+index, a12->length-index));
})

// 8.16.3
NONDET_PREDICATE(sub_atom, 5, (word atom, word before, word length, word after, word subatom, word backtrack)
{
   assert(0);
})

// 8.16.4
PREDICATE(atom_chars, 2, (word atom, word chars)
{
   if (TAGOF(atom) == VARIABLE_TAG)
   {
      // We have to traverse this twice to determine how big a buffer to allocate
      int i = 0;
      if (!must_be_bound(chars))
         return ERROR;
      word w = chars;
      while (TAGOF(w) == COMPOUND_TAG && FUNCTOROF(w) == listFunctor)
      {
         i++;
         if (!must_be_bound(ARGOF(w, 0)))
            return ERROR;
         if (!must_be_character(ARGOF(w, 0)))
            return ERROR;
         w = ARGOF(w, 1);
      }
      if (w != emptyListAtom)
         return type_error(listAtom, chars);
      char* buffer = malloc(i);
      i = 0;
      w = chars;
      while (TAGOF(w) == COMPOUND_TAG && FUNCTOROF(w) == listFunctor)
      {
         buffer[i++] = getConstant(ARGOF(w,0)).data.atom_data->data[0];
         w = ARGOF(w, 1);
      }
      w = MAKE_NATOM(buffer, i);
      free(buffer);
      return unify(w, atom);
   }
   else if (TAGOF(atom) == CONSTANT_TAG)
   {
      constant c = getConstant(atom);
      if (c.type != ATOM_TYPE)
         return type_error(atomAtom, atom);
      Atom a = c.data.atom_data;
      List list;
      word w;
      init_list(&list);
      for (int i = 0; i < a->length; i++)
         list_append(&list, MAKE_NATOM(&a->data[i], 1));
      w = term_from_list(&list, emptyListAtom);
      free_list(&list);
      return unify(chars, w);
   }
   return type_error(atomAtom, atom);
})

// 8.16.5
PREDICATE(atom_codes, 2, (word atom, word codes)
{
   if (TAGOF(atom) == VARIABLE_TAG)
   {
      // We have to traverse this twice to determine how big a buffer to allocate
      int i = 0;
      if (!must_be_bound(codes))
         return ERROR;
      word w = codes;
      while (TAGOF(w) == COMPOUND_TAG && FUNCTOROF(w) == listFunctor)
      {
         i++;
         if (!must_be_bound(ARGOF(w, 0)))
            return ERROR;
         if (!must_be_positive_integer(ARGOF(w, 0)))
            return ERROR;
         w = ARGOF(w, 1);
      }
      if (w != emptyListAtom)
         return type_error(listAtom, codes);
      char* buffer = malloc(i);
      i = 0;
      w = codes;
      while (TAGOF(w) == COMPOUND_TAG && FUNCTOROF(w) == listFunctor)
      {
         buffer[i++] = (char)getConstant(ARGOF(w,0)).data.integer_data->data;
         w = ARGOF(w, 1);
      }
      w = MAKE_NATOM(buffer, i);
      free(buffer);
      return unify(w, atom);
   }
   else if (TAGOF(atom) == CONSTANT_TAG)
   {
      constant c = getConstant(atom);
      if (c.type != ATOM_TYPE)
         return type_error(atomAtom, atom);
      Atom a = c.data.atom_data;
      List list;
      word w;
      init_list(&list);
      for (int i = 0; i < a->length; i++)
         list_append(&list, MAKE_INTEGER(a->data[i]));
      w = term_from_list(&list, emptyListAtom);
      free_list(&list);
      return unify(codes, w);
   }
   return type_error(atomAtom, atom);
})

// 8.16.6
PREDICATE(char_code, 2, (word ch, word code)
{
   if ((TAGOF(code) == VARIABLE_TAG) && (TAGOF(ch) == VARIABLE_TAG))
   {
      instantiation_error();
   }
   else if (TAGOF(ch) == CONSTANT_TAG)
   {
      if (!must_be_character(ch))
         return ERROR;
      Atom a = getConstant(ch).data.atom_data;
      if (TAGOF(code) != VARIABLE_TAG)
         if (!must_be_positive_integer(code))
            return ERROR;
      if ((TAGOF(code) == VARIABLE_TAG) || (TAGOF(code) == CONSTANT_TAG && getConstant(code).type == INTEGER_TYPE))
         return unify(MAKE_INTEGER(a->data[0]), code);
      representation_error(characterCodeAtom, code);
   }
   if (!must_be_integer(code))
      return ERROR;
   Integer code_obj = getConstant(code).data.integer_data;
   if (code_obj->data < 0)
      representation_error(characterCodeAtom, code);
   char a = (char)code_obj->data;
   return unify(MAKE_NATOM(&a, 1), ch);
})

// 8.16.7
PREDICATE(number_chars, 2, (word number, word chars)
{
   if (TAGOF(chars) != VARIABLE_TAG)
   {
      // We have to try this first since for a given number there could be many possible values for chars. Consider 3E+0, 3.0 etc
      // We have to traverse this twice to determine how big a buffer to allocate
      int i = 0;
      if (!must_be_bound(chars))
         return ERROR;
      word w = chars;
      while (TAGOF(w) == COMPOUND_TAG && FUNCTOROF(w) == listFunctor)
      {
         i++;
         if (!must_be_bound(ARGOF(w, 0)))
            return ERROR;
         if (!must_be_character(ARGOF(w, 0)))
            return ERROR;
         w = ARGOF(w, 1);
      }
      if (w != emptyListAtom)
         return type_error(listAtom, chars);
      char* buffer = malloc(i+1);
      i = 0;
      w = chars;
      while (TAGOF(w) == COMPOUND_TAG && FUNCTOROF(w) == listFunctor)
      {
         buffer[i++] = getConstant(ARGOF(w,0)).data.atom_data->data[0];
         w = ARGOF(w, 1);
      }
      buffer[i] = '\0';
      // Ok, now we have the buffer and it is null-terminated. We can pass it to the parser and see what it makes of it!
      Stream stream = stringBufferStream(buffer, i);
      w = read_term(stream, NULL);
      freeStream(stream);
      free(buffer);
      return unify(w, number);
   }
   else if (TAGOF(number) == CONSTANT_TAG)
   {
      constant c = getConstant(number);
      char buffer[32];
      int rc = 0;
      if (c.type == INTEGER_TYPE)
         rc = snprintf(buffer, 32, "%ld", c.data.integer_data->data);
      else if (c.type == FLOAT_TYPE)
      {
         rc = snprintf(buffer, 32, "%f", c.data.float_data->data);
         assert(rc >= 0 && rc < 31);
         // Trim off any trailing zeroes
         for (; rc >= 2; rc--)
            if (buffer[rc-2] == '.' || buffer[rc-1] != '0')
               break;
      }
      else
         type_error(numberAtom, number);
      assert(rc >= 0 && rc < 31);
      List list;
      init_list(&list);
      for (int i = 0; i < rc; i++)
         list_append(&list, MAKE_NATOM(&buffer[i], 1));
      word w = term_from_list(&list, emptyListAtom);
      free_list(&list);
      return unify(w, chars);
   }
   return type_error(numberAtom, number);
})

// 8.16.8
PREDICATE(number_codes, 2, (word number, word codes)
{
   if (TAGOF(codes) != VARIABLE_TAG)
   {
      // We have to try this first since for a given number there could be many possible values for codes. Consider 3E+0, 3.0 etc
      // We have to traverse this twice to determine how big a buffer to allocate
      int i = 0;
      if (!must_be_bound(codes))
         return ERROR;
      word w = codes;
      while (TAGOF(w) == COMPOUND_TAG && FUNCTOROF(w) == listFunctor)
      {
         i++;
         if (!must_be_bound(ARGOF(w, 0)))
            return ERROR;
         if (!must_be_positive_integer(ARGOF(w, 0)))
            return ERROR;
         w = ARGOF(w, 1);
      }
      if (w != emptyListAtom)
         return type_error(listAtom, codes);
      char* buffer = malloc(i+1);
      i = 0;
      w = codes;
      while (TAGOF(w) == COMPOUND_TAG && FUNCTOROF(w) == listFunctor)
      {
         buffer[i++] = (char)getConstant(ARGOF(w,0)).data.integer_data->data;
         w = ARGOF(w, 1);
      }
      buffer[i] = '\0';
      // Ok, now we have the buffer and it is null-terminated. We can pass it to the parser and see what it makes of it!
      Stream stream = stringBufferStream(buffer, i);
      w = read_term(stream, NULL);
      freeStream(stream);
      free(buffer);
      return unify(w, number);
   }
   else if (TAGOF(number) == CONSTANT_TAG)
   {
      constant c = getConstant(number);
      char buffer[32];
      int rc = 0;
      if (c.type == INTEGER_TYPE)
         rc = snprintf(buffer, 32, "%ld", c.data.integer_data->data);
      else if (c.type == FLOAT_TYPE)
      {
         rc = snprintf(buffer, 32, "%f", c.data.float_data->data);
         assert(rc >= 0 && rc < 31);
         // Trim off any trailing zeroes
         for (; rc >= 2; rc--)
            if (buffer[rc-2] == '.' || buffer[rc-1] != '0')
               break;
      }
      else
         type_error(numberAtom, number);
      assert(rc >= 0 && rc < 31);
      List list;
      init_list(&list);
      for (int i = 0; i < rc; i++)
         list_append(&list, MAKE_INTEGER(buffer[i]));
      word w = term_from_list(&list, emptyListAtom);
      free_list(&list);
      return unify(w, codes);
   }
   return type_error(numberAtom, number);
})

// 8.17.1
PREDICATE(set_prolog_flag, 2, (word flag, word value)
{
   if (!must_be_atom(flag) || !must_be_bound(value))
      return ERROR;
   return set_prolog_flag(getConstant(flag).data.atom_data->data, value);
})

// 8.17.2
NONDET_PREDICATE(current_prolog_flag, 2, (word flag, word value, word backtrack)
{
   if (TAGOF(flag) != VARIABLE_TAG)
   {
      if (TAGOF(flag) == CONSTANT_TAG)
      {
         constant c = getConstant(flag);
         if (c.type == ATOM_TYPE)
            return unify(value, get_prolog_flag(c.data.atom_data->data));
      }
      return type_error(prologFlagAtom, flag);
   }
   word list = backtrack==0?prolog_flag_keys():backtrack;
   if (ARGOF(list, 1) != emptyListAtom)
      make_foreign_choicepoint(ARGOF(list, 1));
   return unify(flag, ARGOF(list,0)) && unify(value, get_prolog_flag(getConstant(ARGOF(list, 0)).data.atom_data->data));
})

PREDICATE(halt, 0, ()
{
   halt(0);
   return SUCCESS;
})

PREDICATE(halt, 1, (word a)
{
   if (!must_be_integer(a))
      return ERROR;
   halt((int)getConstant(a).data.integer_data->data);
   return SUCCESS;
})

/// Non-ISO


NONDET_PREDICATE(between, 3, (word low, word high, word value, word backtrack)
{
   if (!(must_be_integer(low) && must_be_integer(high)))
      return FAIL;
   long _low = getConstant(low).data.integer_data->data;
   long _high = getConstant(high).data.integer_data->data;
   if ((TAGOF(value) == VARIABLE_TAG) && _high > _low)
   {
      // Nondet case
      long current;
      if (backtrack == 0)
         current = 0;
      else
         current = getConstant(backtrack).data.integer_data->data;
      //printf("Current: %d (out of %d)\n", current, _high);
      if (current+1 <= _high)
         make_foreign_choicepoint(MAKE_INTEGER(current+1));
      return unify(value, MAKE_INTEGER(current));
   }
   else if (_high == _low)
   {
      // Det case
      return unify(value, low);
   }
   else if (TAGOF(value) == CONSTANT_TAG)
   {
      // Det case
      constant c = getConstant(value);
      if (c.type == INTEGER_TYPE)
      {
         return c.data.integer_data->data >= _high && c.data.integer_data->data <= _low;
      }
      return type_error(integerAtom, value);
   }
   else
      return type_error(integerAtom, value);
})


