#include "global.h"
#include "kernel.h"
#include "errors.h"
#include "stream.h"
#include "parser.h"
#include "options.h"
#include "operators.h"
#include "prolog_flag.h"
#include "arithmetic.h"
#include "char_conversion.h"
#include "term_writer.h"
#include "record.h"
#include "string_builder.h"
#include <stdio.h>
#include <ctype.h>

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
   return TAGOF(a) == CONSTANT_TAG && getConstantType(a) == ATOM_TYPE;
})

// 8.3.3
PREDICATE(integer, 1, (word a)
{
   return TAGOF(a) == CONSTANT_TAG && getConstantType(a) == INTEGER_TYPE;
})

// 8.3.4
PREDICATE(float, 1, (word a)
{
   return TAGOF(a) == CONSTANT_TAG && getConstantType(a) == FLOAT_TYPE;
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
   int c;
   if (TAGOF(a) == CONSTANT_TAG)
   {
      c = getConstantType(a);
      return (c == INTEGER_TYPE || c == FLOAT_TYPE || c == BIGINTEGER_TYPE || c == RATIONAL_TYPE);
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
            must_be_atomic(name)))
      {
         return ERROR;
      }
      long a = getConstant(arity, NULL).integer_data;
      if (a == 0)
         return unify(term, name);
      if (a > 0 && !must_be_atom(name))
         return ERROR;
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
      Functor f = getConstant(FUNCTOROF(term), NULL).functor_data;
      return unify(name, f->name) && unify(arity, MAKE_INTEGER(f->arity));
   }
   return type_error(compoundAtom, term);
})

// 8.5.2
NONDET_PREDICATE(arg, 3, (word n, word term, word arg, word backtrack)
{
   if (!must_be_bound(term))
      return ERROR;
   if (TAGOF(term) != COMPOUND_TAG)
      return type_error(compoundAtom, term);
   if (TAGOF(n) == VARIABLE_TAG)
   {
      // -,+,? mode
      Functor functor = getConstant(FUNCTOROF(term), NULL).functor_data;
      long index = backtrack == 0?0:getConstant(backtrack, NULL).integer_data;
      if (index + 1 < functor->arity)
         make_foreign_choicepoint(MAKE_INTEGER(index+1));
      if (index >= functor->arity)
         return 0;
      return (unify(n, MAKE_INTEGER(index+1)) && // arg is 1-based, but all our terms are 0-based
              unify(ARGOF(term, index), arg));
   }
   if (!must_be_positive_integer(n))
      return 0;
   long _n = getConstant(n, NULL).integer_data;
   Functor functor = getConstant(FUNCTOROF(term), NULL).functor_data;
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
      Functor f = getConstant(FUNCTOROF(term), NULL).functor_data;
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
      if (list_length(&list) == 0)
      {
         free_list(&list);
         return domain_error(nonEmptyListAtom, univ);
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
   word w;
   if (evaluate_term(expr, &w))
      return unify(w, result);
   return ERROR;
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
      int c = getConstantType(body);
      if (c == INTEGER_TYPE || c == FLOAT_TYPE)
         return type_error(callableAtom, body);
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
   if ((p->flags & PREDICATE_DYNAMIC) == 0)
      return permission_error(accessAtom, privateProcedureAtom, predicate_indicator(head));
   word list;
   if (backtrack == 0)
      list = term_from_list(&p->clauses, emptyListAtom);
   else
      list = backtrack;
   if (list == emptyListAtom)
      return FAIL;
   if (TAGOF(list) == COMPOUND_TAG && FUNCTOROF(list) == listFunctor)
      make_foreign_choicepoint(ARGOF(list,1));
   word clause = ARGOF(list, 0);
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
   // FIXME: assumes current module
   if (TAGOF(indicator) != VARIABLE_TAG)
   {
      if ((!(TAGOF(indicator) == COMPOUND_TAG && FUNCTOROF(indicator) == predicateIndicatorFunctor)))
         return type_error(predicateIndicatorAtom, indicator);
      if ((TAGOF(ARGOF(indicator, 0)) != VARIABLE_TAG) && !(TAGOF(ARGOF(indicator, 0)) == CONSTANT_TAG && getConstantType(ARGOF(indicator, 0)) == ATOM_TYPE))
         return type_error(predicateIndicatorAtom, indicator);
      if ((TAGOF(ARGOF(indicator, 1)) != VARIABLE_TAG) && !(TAGOF(ARGOF(indicator, 1)) == CONSTANT_TAG && getConstantType(ARGOF(indicator, 1)) == INTEGER_TYPE))
         return type_error(predicateIndicatorAtom, indicator);
   }
   // Now indicator is either unbound or bound to /(A,B)
   // This is extremely inefficient - if indicator is ground then this is deterministic!
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
   {
      predicates = backtrack;
   }
   if (ARGOF(predicates, 1) != emptyListAtom)
   {
      make_foreign_choicepoint(ARGOF(predicates, 1));
   }
   return unify(indicator, predicate_indicator(ARGOF(predicates, 0)));

})

// 8.9.1
PREDICATE(asserta, 1, (word term)
{
   Module module;
   word clause;
   if (TAGOF(term) == VARIABLE_TAG)
      return instantiation_error();
   strip_module(term, &clause, &module);
   //printf("asserta:\n");
   //PORTRAY(clause); printf("\n");

   asserta(module, clause);
   return 1;
})

// 8.9.2
PREDICATE(assertz, 1, (word term)
{
   Module module;
   word clause;
   if (TAGOF(term) == VARIABLE_TAG)
      return instantiation_error();
   strip_module(term, &clause, &module);
   assertz(module, clause);
   return 1;
})

// 8.9.3
NONDET_PREDICATE(retract, 1, (word term, word backtrack)
{
   word list;
   Module m;
   word clause;
   if (TAGOF(term) == COMPOUND_TAG && FUNCTOROF(term) == crossModuleCallFunctor)
   {
      m = find_module(ARGOF(term, 0));
      if (m == NULL)
         return FAIL; // If the module does not exist it obviously does not contain any clauses
      clause = ARGOF(term, 1);
   }
   else
   {
      m = get_current_module();
      clause = term;
   }
   if (backtrack == 0)
   {
      word functor;
      if (!head_functor(term, &m, &functor))
         return ERROR;
      Predicate p = lookup_predicate(m, functor);
      if (p == NULL)
      {
         return FAIL; // Cannot retract it if it does not exist
      }
      if ((p->flags & PREDICATE_DYNAMIC) == 0)
         return permission_error(modifyAtom, staticProcedureAtom, predicate_indicator(functor));
      list = term_from_list(&p->clauses, emptyListAtom);
   }
   else
      list = backtrack;
   if (list == emptyListAtom)
      return FAIL;
   if (TAGOF(list) == COMPOUND_TAG && FUNCTOROF(list) == listFunctor && ARGOF(list, 1) != emptyListAtom)
   {
      make_foreign_choicepoint(ARGOF(list, 1));
   }
   if (unify(ARGOF(list, 0), term))
   {
      retract(m, term);
      return SUCCESS;
   }
   else
      return FAIL;
})

// 8.9.4
PREDICATE(abolish, 1, (word indicator)
{
   Module m;
   if (TAGOF(indicator) == COMPOUND_TAG && FUNCTOROF(indicator) == crossModuleCallFunctor)
   {
      m = find_module(ARGOF(indicator, 0));
      if (m == NULL)
         return SUCCESS; // maybe should be existence error?
      return abolish(m, ARGOF(indicator, 1));
   }
   return abolish(get_current_module(), indicator);
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
   Stream s = fileStream(getConstant(source_sink, NULL).atom_data->data, io_mode, &options);
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
   Stream s = fileStream(getConstant(source_sink, NULL).atom_data->data, io_mode, &_options);
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
   long index = backtrack==0?0:getConstant(backtrack, NULL).integer_data;
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
   return s->seek(s, (int)getConstant(position, NULL).integer_data, SEEK_SET) >= 0;
})

// 8.12.1 get_char/1,2 get_code/1,2
PREDICATE(get_char, 1, (word c)
{
   char ch = _getch(current_input);
   return unify(c, MAKE_NATOM(&ch, 1));
})
PREDICATE(get_char, 2, (word stream, word c)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   char ch = _getch(s);
   return unify(c, MAKE_NATOM(&ch, 1));
})
PREDICATE(get_code, 1, (word code)
{
   return unify(code, MAKE_INTEGER(_getch(current_input)));
})
PREDICATE(get_code, 2, (word stream, word code)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   return unify(code, MAKE_INTEGER(_getch(s)));
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
   putch(current_output, getConstant(c, NULL).atom_data->data[0]);
   return SUCCESS;
})
PREDICATE(put_char, 2, (word stream, word c)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   if (!must_be_character(c)) return ERROR;
   putch(s, getConstant(c, NULL).atom_data->data[0]);
   return SUCCESS;
})
PREDICATE(put_code, 1, (word code)
{
   if (!must_be_positive_integer(code)) return ERROR;
   putch(current_output, (int)getConstant(code, NULL).integer_data);
   return SUCCESS;
})
PREDICATE(put_code, 2, (word stream, word code)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   if (!must_be_positive_integer(code)) return ERROR;
   putch(s, getConstant(code, NULL).integer_data);
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
   putb(current_output, (int)getConstant(c, NULL).integer_data);
   return SUCCESS;
})
PREDICATE(put_byte, 2, (word stream, word c)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   if (!must_be_positive_integer(c)) return ERROR;
   putb(s, getConstant(c, NULL).integer_data);
   return SUCCESS;
})

// 8.14.1 read_term/2,3 read/1,2
PREDICATE(read_term, 2, (word term, word options)
{
   Options _options;
   init_options(&_options);
   options_from_term(&_options, options);
   word t;
   int rc = read_term(current_input, &_options, &t);
   free_options(&_options);
   return rc && unify(term, t);
})
PREDICATE(read_term, 3, (word stream, word term, word options)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   Options _options;
   init_options(&_options);
   options_from_term(&_options, options);
   word t;
   int rc = read_term(s, &_options, &t);
   free_options(&_options);
   return rc && unify(term, t);
})
PREDICATE(read, 1, (word term)
{
   Options _options;
   init_options(&_options);
   word t;
   int rc = read_term(current_input, &_options, &t);
   free_options(&_options);
   return rc && unify(term, t);
})
PREDICATE(read, 2, (word stream, word term)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   Options _options;
   init_options(&_options);
   word t;
   int rc = read_term(s, &_options, &t);
   //printf("Read this term\n"); PORTRAY(t); printf("\n");
   free_options(&_options);
   return rc && unify(term, t);
})

// 8.14.2 write_term/2,3 write/1,2, writeq/1,2, write_canonical/1,2
PREDICATE(write_term, 2, (word term, word options)
{
   Options _options;
   init_options(&_options);
   options_from_term(&_options, options);
   int rc = write_term(current_output, term, &_options);
   free_options(&_options);
   return rc;
})

PREDICATE(write_term, 3, (word stream, word term, word options)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   Options _options;
   init_options(&_options);
   options_from_term(&_options, options);
   int rc = write_term(s, term, &_options);
   free_options(&_options);
   return rc;
})

PREDICATE(write, 1, (word term)
{
   Options _options;
   init_options(&_options);
   set_option(&_options, numbervarsAtom, trueAtom);
   int rc = write_term(current_output, term, &_options);
   free_options(&_options);
   return rc;
})

PREDICATE(write, 2, (word stream, word term)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   Options _options;
   init_options(&_options);
   set_option(&_options, numbervarsAtom, trueAtom);
   int rc = write_term(s, term, &_options);
   free_options(&_options);
   return rc;
})

PREDICATE(writeq, 1, (word term)
{
   Options _options;
   init_options(&_options);
   set_option(&_options, numbervarsAtom, trueAtom);
   set_option(&_options, quotedAtom, trueAtom);
   int rc = write_term(current_output, term, &_options);
   free_options(&_options);
   return rc;
})

PREDICATE(writeq, 2, (word stream, word term)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   Options _options;
   init_options(&_options);
   set_option(&_options, numbervarsAtom, trueAtom);
   set_option(&_options, quotedAtom, trueAtom);
   int rc = write_term(s, term, &_options);
   free_options(&_options);
   return rc;
})

PREDICATE(write_canonical, 1, (word term)
{
   Options _options;
   init_options(&_options);
   set_option(&_options, ignoreOpsAtom, trueAtom);
   set_option(&_options, quotedAtom, trueAtom);
   int rc = write_term(current_output, term, &_options);
   free_options(&_options);
   return rc;
})

PREDICATE(write_canonical, 2, (word stream, word term)
{
   Stream s = get_stream(stream);
   if (s == NULL)
      return ERROR;
   Options _options;
   init_options(&_options);
   set_option(&_options, ignoreOpsAtom, trueAtom);
   set_option(&_options, quotedAtom, trueAtom);
   int rc = write_term(s, term, &_options);
   free_options(&_options);
   return rc;
})


// 8.14.3 op/3
PREDICATE(op, 3, (word priority, word fixity, word op)
{
   // FIXME: Should support a list as op, apparently
   if (!must_be_atom(fixity)) return ERROR;
   if (!must_be_atom(op)) return ERROR;
   if (!must_be_integer(priority)) return ERROR;
   Atom op_c = getConstant(op, NULL).atom_data;
   int pri = (int)getConstant(priority, NULL).integer_data;
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
   CharConversionTable[getConstant(in_char, NULL).atom_data->data[0]] = getConstant(out_char, NULL).atom_data->data[0];
   return SUCCESS;
})
// 8.14.6 current_char_conversion/2
NONDET_PREDICATE(current_char_conversion, 2, (word in_char, word out_char, word backtrack)
{
   if (TAGOF(in_char) == CONSTANT_TAG)
   {
      int ti;
      cdata ci = getConstant(in_char, &ti);
      if (ti == ATOM_TYPE)
         return unify(out_char, MAKE_NATOM(&ci.atom_data->data[0], 1));
      return type_error(characterAtom, in_char);
   }
   else if (TAGOF(in_char) == VARIABLE_TAG)
   {
      if (TAGOF(out_char) == CONSTANT_TAG)
      {
         int to;
         cdata co = getConstant(in_char, &to);
         if (to == ATOM_TYPE)
         {
            char needle = co.atom_data->data[0];
            // We have to search the table to see if something matches
            int startindex = 0;
            if (backtrack != 0)
               startindex = (int)getConstant(backtrack, NULL).integer_data;
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
            index = (int)getConstant(backtrack, NULL).integer_data;
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
      return type_error(atomAtom, atom);
   int type;
   cdata c = getConstant(atom, &type);
   if (type != ATOM_TYPE)
      return type_error(atomAtom, atom);
   if (TAGOF(length) != VARIABLE_TAG && !(must_be_integer(length)))
      return ERROR;
   if (TAGOF(length) == CONSTANT_TAG && !(must_be_positive_integer(length)))
      return ERROR;
   return unify(length, MAKE_INTEGER(c.atom_data->length));
})

// 8.1.6.2
NONDET_PREDICATE(atom_concat, 3, (word atom1, word atom2, word atom12, word backtrack)
{
   long index = 0;
   if (backtrack == 0)
   { // First call
      if (TAGOF(atom1) == VARIABLE_TAG && TAGOF(atom12) == VARIABLE_TAG)
         return instantiation_error();
      if (TAGOF(atom2) == VARIABLE_TAG && TAGOF(atom12) == VARIABLE_TAG)
         return instantiation_error();
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
         Atom a1 = getConstant(atom1, NULL).atom_data;
         Atom a2 = getConstant(atom2, NULL).atom_data;
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
   {
      index = getConstant(backtrack, NULL).integer_data;
   }
   Atom a12 = getConstant(atom12, NULL).atom_data;
   if (index == a12->length+1)
      return FAIL;
   make_foreign_choicepoint(MAKE_INTEGER(index+1));
   return unify(atom1, MAKE_NATOM(a12->data, index)) && unify(atom2, MAKE_NATOM(a12->data+index, a12->length-index));
})

// 8.16.3
NONDET_PREDICATE(sub_atom, 5, (word atom, word before, word length, word after, word subatom, word backtrack)
{
   if (!must_be_atom(atom))
      return ERROR;
   if (TAGOF(before) != VARIABLE_TAG && !must_be_integer(before))
      return ERROR;
   if (TAGOF(length) != VARIABLE_TAG && !must_be_integer(length))
      return ERROR;
   if (TAGOF(after) != VARIABLE_TAG && !must_be_integer(after))
      return ERROR;
   if (TAGOF(subatom) != VARIABLE_TAG && !must_be_atom(subatom))
      return ERROR;
   Atom input = getConstant(atom, NULL).atom_data;
   long _start = 0;
   long fixed_start = 0;
   long _length = 0;
   long fixed_length = 0;
   long _remaining = 0;
   long fixed_remaining = 0;
   if (backtrack == 0)
   {
      // First call
      if (TAGOF(before) == CONSTANT_TAG)
      {
         fixed_start = 1;
         _start = getConstant(before, NULL).integer_data;
      }
      if (TAGOF(length) == CONSTANT_TAG)
      {
         fixed_length = 1;
         _length = getConstant(length, NULL).integer_data;
      }
      if (TAGOF(after) == CONSTANT_TAG)
      {
         fixed_remaining = 1;
         _remaining = getConstant(after, NULL).integer_data;
      }
      if (fixed_start && fixed_remaining && !fixed_length)
      {
         // Deterministic. Will end up in the general case below
         _length = input->length - _start - _remaining;
         fixed_length = 1;
      }
      if (fixed_remaining && fixed_length && !fixed_start)
      {
         // Deterministic. Will end up in the general case below
         _start = input->length - _length - _remaining;
         fixed_start = 1;
      }
      if (fixed_start && fixed_length)
      {
         // General deterministic case
         return unify(after, MAKE_INTEGER(input->length - _start - _length)) &&
            unify(before, MAKE_INTEGER(_start)) &&
            unify(length, MAKE_INTEGER(_length)) &&
            unify(subatom, MAKE_NATOM(&input->data[_start], _length));
      }
   }
   else
   {
      // Redo
      _start = getConstant(ARGOF(backtrack, 0), NULL).integer_data;
      fixed_start = getConstant(ARGOF(backtrack, 1), NULL).integer_data;
      _length = getConstant(ARGOF(backtrack, 2), NULL).integer_data;
      fixed_length = getConstant(ARGOF(backtrack, 3), NULL).integer_data;
      _remaining = getConstant(ARGOF(backtrack, 4), NULL).integer_data;
      fixed_remaining = getConstant(ARGOF(backtrack, 5), NULL).integer_data;
      if (!fixed_length)
      {
         _length++;
         if (_start + _length > input->length)
         {
            _length = 0;
            if (!fixed_start)
            {
               _start++;
               if (_start > input->length)
               {
                  return FAIL;
               }
            }
            else
            {
               // start is fixed, so length and remaining are free
               // but remaining is always just computed
               return FAIL;
            }
         }
      }
      else
      {
         // length is fixed, so start and remaining must be free
         _start++;
         _remaining--;
         if (_length + _start > input->length)
         {
            return FAIL;
         }
      }
   }
   make_foreign_choicepoint(MAKE_VCOMPOUND(subAtomContextFunctor, MAKE_INTEGER(_start), MAKE_INTEGER(fixed_start), MAKE_INTEGER(_length), MAKE_INTEGER(fixed_length), MAKE_INTEGER(_remaining), MAKE_INTEGER(fixed_remaining)));
   return unify(after, MAKE_INTEGER(input->length - _start - _length)) &&
      unify(before, MAKE_INTEGER(_start)) &&
      unify(length, MAKE_INTEGER(_length)) &&
      unify(subatom, MAKE_NATOM(&input->data[_start], _length));
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
      if (TAGOF(w) == VARIABLE_TAG)
         return instantiation_error();
      if (w != emptyListAtom)
         return type_error(listAtom, chars);
      char* buffer = malloc(i);
      i = 0;
      w = chars;
      while (TAGOF(w) == COMPOUND_TAG && FUNCTOROF(w) == listFunctor)
      {
         buffer[i++] = getConstant(ARGOF(w,0), NULL).atom_data->data[0];
         w = ARGOF(w, 1);
      }
      w = MAKE_NATOM(buffer, i);
      free(buffer);
      return unify(w, atom);
   }
   else if (TAGOF(atom) == CONSTANT_TAG)
   {
      int type;
      cdata c = getConstant(atom, &type);
      if (type != ATOM_TYPE)
         return type_error(atomAtom, atom);
      Atom a = c.atom_data;
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
         if (!must_be_character_code(ARGOF(w, 0)))
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
         buffer[i++] = (char)getConstant(ARGOF(w,0), NULL).integer_data;
         w = ARGOF(w, 1);
      }
      w = MAKE_NATOM(buffer, i);
      free(buffer);
      return unify(w, atom);
   }
   else if (TAGOF(atom) == CONSTANT_TAG)
   {
      int type;
      cdata c = getConstant(atom, &type);
      if (type != ATOM_TYPE)
         return type_error(atomAtom, atom);
      Atom a = c.atom_data;
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
      return instantiation_error();
   }
   else if (TAGOF(ch) == CONSTANT_TAG)
   {
      if (!must_be_character(ch))
         return ERROR;
      Atom a = getConstant(ch, NULL).atom_data;
      if (TAGOF(code) != VARIABLE_TAG)
         if (!must_be_positive_integer(code))
            return ERROR;
      if ((TAGOF(code) == VARIABLE_TAG) || (TAGOF(code) == CONSTANT_TAG && getConstantType(code) == INTEGER_TYPE))
         return unify(MAKE_INTEGER(a->data[0]), code);
      return representation_error(characterCodeAtom, code);
   }
   if (!must_be_integer(code))
      return ERROR;
   long code_int = getConstant(code, NULL).integer_data;
   if (code_int < 0)
      return representation_error(characterCodeAtom, code);
   char a = (char)code_int;
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
         buffer[i++] = getConstant(ARGOF(w,0), NULL).atom_data->data[0];
         w = ARGOF(w, 1);
      }
      buffer[i] = '\0';
      // Ok, now we have the buffer and it is null-terminated. We can pass it to the parser and see what it makes of it!
      Stream stream = stringBufferStream(buffer, i);
      //int rc = read_term(stream, NULL, &w);
      Token token = lex(stream);
      int rc = peek_raw_char(stream) == -1;
      freeStream(stream);
      free(buffer);
      if (!rc)
      {
         freeToken(token);
         return syntax_error(MAKE_ATOM("illegal number"));
      }
      switch (token->type)
      {
         case IntegerTokenType:
            rc &= unify(number, MAKE_INTEGER(token->data.integer_data));
            freeToken(token);
            break;
         case BigIntegerTokenType:
         {
            mpz_t m;
            mpz_init_set_str(m, token->data.biginteger_data, 10);
            rc &= unify(number, MAKE_BIGINTEGER(m));
            freeToken(token);
            break;
         }
         case FloatTokenType:
         {
            rc &= unify(number, MAKE_FLOAT(token->data.float_data));
            freeToken(token);
            break;
         }
         default:
            freeToken(token);
            return syntax_error(MAKE_ATOM("illegal number"));
      }
      return rc;
   }
   else if (TAGOF(number) == CONSTANT_TAG)
   {
      int type;
      cdata c = getConstant(number, &type);
      char buffer[32];
      int rc = 0;
      if (type == INTEGER_TYPE)
         rc = snprintf(buffer, 32, "%ld", c.integer_data);
      else if (type == FLOAT_TYPE)
      {
         rc = snprintf(buffer, 32, "%f", c.float_data->data);
         assert(rc >= 0 && rc < 31);
         // Trim off any trailing zeroes
         for (; rc >= 2; rc--)
            if (buffer[rc-2] == '.' || buffer[rc-1] != '0')
               break;
      }
      else
         return type_error(numberAtom, number);
      assert(rc >= 0 && rc < 31);
      List list;
      init_list(&list);
      for (int i = 0; i < rc; i++)
         list_append(&list, MAKE_NATOM(&buffer[i], 1));
      word w = term_from_list(&list, emptyListAtom);
      free_list(&list);
      return unify(w, chars);
   }
   else if (TAGOF(number) == VARIABLE_TAG)
      return instantiation_error();
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
         if (!must_be_character_code(ARGOF(w, 0)))
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
         buffer[i++] = (char)getConstant(ARGOF(w,0), NULL).integer_data;
         w = ARGOF(w, 1);
      }
      buffer[i] = '\0';
      // Ok, now we have the buffer and it is null-terminated. We can pass it to the parser and see what it makes of it!
      Stream stream = stringBufferStream(buffer, i);
      //int rc = read_term(stream, NULL, &w);
      Token token = lex(stream);
      int rc = peek_raw_char(stream) == -1;
      freeStream(stream);
      free(buffer);
      if (!rc)
      {
         freeToken(token);
         return syntax_error(MAKE_ATOM("illegal number"));
      }
      switch (token->type)
      {
         case IntegerTokenType:
            rc &= unify(number, MAKE_INTEGER(token->data.integer_data));
            freeToken(token);
            break;
         case BigIntegerTokenType:
         {
            mpz_t m;
            mpz_init_set_str(m, token->data.biginteger_data, 10);
            rc &= unify(number, MAKE_BIGINTEGER(m));
            freeToken(token);
            break;
         }
         case FloatTokenType:
         {
            rc &= unify(number, MAKE_FLOAT(token->data.float_data));
            freeToken(token);
            break;
         }
         default:
            freeToken(token);
            return syntax_error(MAKE_ATOM("illegal number"));
      }
      return rc;
   }
   else if (TAGOF(number) == CONSTANT_TAG)
   {
      int type;
      cdata c = getConstant(number, &type);
      char buffer[32];
      int rc = 0;
      if (type == INTEGER_TYPE)
         rc = snprintf(buffer, 32, "%ld", c.integer_data);
      else if (type == FLOAT_TYPE)
      {
         rc = snprintf(buffer, 32, "%f", c.float_data->data);
         assert(rc >= 0 && rc < 31);
         // Trim off any trailing zeroes
         for (; rc >= 2; rc--)
            if (buffer[rc-2] == '.' || buffer[rc-1] != '0')
               break;
      }
      else
         return type_error(numberAtom, number);
      assert(rc >= 0 && rc < 31);
      List list;
      init_list(&list);
      for (int i = 0; i < rc; i++)
         list_append(&list, MAKE_INTEGER(buffer[i]));
      word w = term_from_list(&list, emptyListAtom);
      free_list(&list);
      return unify(w, codes);
   }
   else if (TAGOF(number) == VARIABLE_TAG)
      return instantiation_error();
   return type_error(numberAtom, number);
})

// 8.17.1
PREDICATE(set_prolog_flag, 2, (word flag, word value)
{
   if (!must_be_atom(flag) || !must_be_bound(value))
      return ERROR;
   return set_prolog_flag(flag, value);
})

// 8.17.2
NONDET_PREDICATE(current_prolog_flag, 2, (word flag, word value, word backtrack)
{
   if (TAGOF(flag) != VARIABLE_TAG)
   {
      if (TAGOF(flag) == CONSTANT_TAG)
      {
         int type;
         cdata c = getConstant(flag, &type);
         if (type == ATOM_TYPE)
         {
            word v = get_prolog_flag(c.atom_data->data);
            if (v == 0)
               return domain_error(prologFlagAtom, flag);
            return unify(value, v);
         }
      }
      return type_error(atomAtom, flag);
   }
   word list = backtrack==0?prolog_flag_keys():backtrack;
   if (ARGOF(list, 1) != emptyListAtom)
      make_foreign_choicepoint(ARGOF(list, 1));
   if (!unify(flag, ARGOF(list,0)))
      return FAIL;
   word v = get_prolog_flag(getConstant(ARGOF(list, 0), NULL).atom_data->data);
   if (v == 0) // This really shouldnt happen since we were the ones who made the key!
      return existence_error(prologFlagAtom, flag);
   return unify(value, v);
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
   halt((int)getConstant(a, NULL).integer_data);
   return SUCCESS;
})

/// Non-ISO

// Recorded database
PREDICATE(recorda, 3, (word key, word term, word ref)
{
   if (TAGOF(key) == CONSTANT_TAG)
      return unify(ref, recorda(key, term));
   else if (TAGOF(key) == COMPOUND_TAG)
      return unify(ref, recorda(FUNCTOROF(key), term));
   else if (TAGOF(key) == VARIABLE_TAG)
      return instantiation_error();
   assert(0);
})

PREDICATE(recordz, 3, (word key, word term, word ref)
{
   if (TAGOF(key) == CONSTANT_TAG)
      return unify(ref, recordz(key, term));
   else if (TAGOF(key) == COMPOUND_TAG)
      return unify(ref, recordz(FUNCTOROF(key), term));
   else if (TAGOF(key) == VARIABLE_TAG)
      return instantiation_error();
   assert(0);
})

PREDICATE(erase, 1, (word ref)
{
   return _erase(ref);
})


NONDET_PREDICATE(recorded, 3, (word key, word value, word ref, word backtrack)
{
   if (TAGOF(ref) == POINTER_TAG)
   {
      // Deterministic case
      word k;
      word v;
      return recorded(ref, &k, &v) && unify(k, key) && unify(v, value);
   }
   else
   {
      if (TAGOF(ref) == VARIABLE_TAG)
      {
         word list;
         if (backtrack == 0)
         {
            if (TAGOF(key) == CONSTANT_TAG)
               list = find_records(key);
            else if (TAGOF(key) == COMPOUND_TAG)
               return list = find_records(FUNCTOROF(key));
            else if (TAGOF(key) == VARIABLE_TAG)
               return instantiation_error();
            else
               return type_error(dbReferenceAtom, key);
         }
         else
            list = backtrack;
         if (list == emptyListAtom)
            return FAIL;
         if (ARGOF(list, 1) != emptyListAtom)
         {
            make_foreign_choicepoint(ARGOF(list, 1));
         }
         word head = ARGOF(list, 0);
         // DANGER! head contains references to the local storage! We must copy it to the heap or it will no longer be valid after erased!
         return unify(value, copy_term(ARGOF(head, 0))) && unify(ref, ARGOF(head, 1));
      }
   }
   return type_error(dbReferenceAtom, key);
})


NONDET_PREDICATE(between, 3, (word low, word high, word value, word backtrack)
{
   if (!must_be_integer(low) || !must_be_integer(high))
      return FAIL;
   long _low = getConstant(low, NULL).integer_data;
   long _high = getConstant(high, NULL).integer_data;
   if ((TAGOF(value) == VARIABLE_TAG) && _high >= _low)
   {
      // Nondet case
      long current;
      if (backtrack == 0)
         current = _low;
      else
         current = getConstant(backtrack, NULL).integer_data;
      //printf("Current: %d (out of %d)\n", current, _high);
      if (current+1 <= _high)
         make_foreign_choicepoint(MAKE_INTEGER(current+1));
      return unify(value, MAKE_INTEGER(current));
   }
   else if (_high < _low)
   {
      return FAIL;
   }
   else if (_high == _low)
   {
      // Det case
      return unify(value, low);
   }
   else if (TAGOF(value) == CONSTANT_TAG)
   {
      // Det case
      int type;
      cdata c = getConstant(value, &type);
      if (type == INTEGER_TYPE)
      {
         return c.integer_data >= _low && c.integer_data <= _high;
      }
      return type_error(integerAtom, value);
   }
   else
      return type_error(integerAtom, value);
})

PREDICATE(keysort, 2, (word in, word out)
{
   // Lets just use qsort for now
   if (in == emptyListAtom)
      return unify(out, emptyListAtom);


   // First count the number of entries in in
   int i = 0;
   word list = in;
   while (TAGOF(list) == COMPOUND_TAG && FUNCTOROF(list) == listFunctor)
   {
      i++;
      list = ARGOF(list,1);
   }
   // Now make an array
   word* array = malloc(sizeof(word)*i);
   // And fill it in
   list = in;
   i = 0;
   while (TAGOF(list) == COMPOUND_TAG && FUNCTOROF(list) == listFunctor)
   {
      array[i++] = ARGOF(list, 0);
      list = ARGOF(list,1);
   }
   // Now sort
   qsort(array, i, sizeof(word), &qcompare_keys);

   // And now make a new list
   list = emptyListAtom;
   while (--i >= 0)
      list = MAKE_VCOMPOUND(listFunctor, array[i], list);
   free(array);
   return unify(out, list);
})

PREDICATE(sort, 2, (word in, word out)
{
   // Lets just use qsort for now
   if (in == emptyListAtom)
      return unify(out, emptyListAtom);


   // First count the number of entries in in
   int i = 0;
   word list = in;
   while (TAGOF(list) == COMPOUND_TAG && FUNCTOROF(list) == listFunctor)
   {
      i++;
      list = ARGOF(list,1);
   }
   // Now make an array
   word* array = malloc(sizeof(word)*i);
   // And fill it in
   list = in;
   i = 0;
   while (TAGOF(list) == COMPOUND_TAG && FUNCTOROF(list) == listFunctor)
   {
      array[i++] = ARGOF(list, 0);
      list = ARGOF(list,1);
   }
   // Now sort
   qsort(array, i, sizeof(word), &qcompare_terms);

   // And now make a new list. We must also remove duplicates. Add the first item to the list here
   // and then remember the last item we added from now on
   word last = array[--i];
   list = MAKE_VCOMPOUND(listFunctor, array[i], emptyListAtom);

   while (--i >= 0)
   {
      if (array[i] != last)
      {
         list = MAKE_VCOMPOUND(listFunctor, array[i], list);
         last = array[i];
      }
   }
   free(array);
   return unify(out, list);
})

PREDICATE(format, 2, (word fmt, word args)
{
    return format(current_output->term, fmt, args);
})

PREDICATE(format, 3, (word sink, word fmt, word args)
{
   return format(sink, fmt, args);
})


PREDICATE($choicepoint_depth, 1, (word t)
{
   return unify(t, get_choicepoint_depth());
})

PREDICATE(upcase_atom, 2, (word in, word out)
{
   if (!must_be_atom(in))
      return ERROR;
   Atom a = getConstant(in, NULL).atom_data;
   char* buffer = malloc(a->length);
   for (int i = 0; i < a->length; i++)
      buffer[i] = toupper(a->data[i]);
   RC rc = unify(out, MAKE_NATOM(buffer, a->length));
   free(buffer);
   return rc;
})

PREDICATE(downcase_atom, 2, (word in, word out)
{
   if (!must_be_atom(in))
      return ERROR;
   Atom a = getConstant(in, NULL).atom_data;
   char* buffer = malloc(a->length);
   for (int i = 0; i < a->length; i++)
      buffer[i] = tolower(a->data[i]);
   RC rc = unify(out, MAKE_NATOM(buffer, a->length));
   free(buffer);
   return rc;
})

PREDICATE(atomic_list_concat, 3, (word atomics, word sep, word out)
{
   if (!must_be_bound(atomics))
      return ERROR;
   if (atomics == emptyListAtom)
      return unify(out, emptyAtom);
   if (!must_be_atom(sep))
      return ERROR;
   cdata csep = getConstant(sep, NULL);
   if (TAGOF(atomics) == COMPOUND_TAG && FUNCTOROF(atomics) == listFunctor)
   {
      char* text;
      int length;
      word w = atomics;
      StringBuilder sb = stringBuilder();
      while (TAGOF(w) == COMPOUND_TAG && FUNCTOROF(w) == listFunctor)
      {
         word h = ARGOF(w, 0);
         w = ARGOF(w, 1);
         if (TAGOF(h) == CONSTANT_TAG)
         {
            int type;
            cdata c = getConstant(h, &type);
            if (type == ATOM_TYPE)
               append_string_no_copy(sb, c.atom_data->data, c.atom_data->length);
            else if (type == FLOAT_TYPE)
            {
               char buffer[64];
               sprintf((char*)buffer, "%f", c.float_data->data);
               append_string(sb, strdup(buffer), strlen(buffer));
            }
            else if (type == INTEGER_TYPE)
            {
               char buffer[64];
               sprintf((char*)buffer, "%ld", c.integer_data);
               append_string(sb, strdup(buffer), strlen(buffer));
            }
            else
            {
               freeStringBuilder(sb);
               return type_error(textAtom, h);
            }
         }
         else
         {
            freeStringBuilder(sb);
            return type_error(textAtom, h);
         }
         if (TAGOF(w) == COMPOUND_TAG && FUNCTOROF(w) == listFunctor && csep.atom_data->length != 0)
            append_string_no_copy(sb, csep.atom_data->data, csep.atom_data->length);
      }
      if (TAGOF(w) == VARIABLE_TAG)
      {
         freeStringBuilder(sb);
         return instantiation_error();
      }
      if (w != emptyListAtom)
      {
         freeStringBuilder(sb);
         return type_error(listAtom, w);
      }
      finalize_buffer(sb, &text, &length);
      return unify(out, MAKE_NATOM(text, length));
   }
   return type_error(listAtom, atomics);
})

NONDET_PREDICATE(code_type, 2, (word code, word type, word backtrack)
{
   if (TAGOF(code) == VARIABLE_TAG && TAGOF(type) == VARIABLE_TAG)
      return instantiation_error();
   if (TAGOF(code) == VARIABLE_TAG)
   {
      // Nondeterministic hard case - find all codes with the given class
      // Not currently implemented
      return instantiation_error();
   }
   else
   {
      if (!must_be_atomic(code))
         return ERROR;
      int ctype;
      cdata c = getConstant(code, &ctype);
      int cval = -1;
      if (ctype == ATOM_TYPE)
      {
         if (c.atom_data->length != 1)
            return type_error(characterAtom, code);
         cval = c.atom_data->data[0];
      }
      else if (!must_be_character_code(code))
         return ERROR;
      else
      {
         cval = c.integer_data;
      }
      if (TAGOF(type) != VARIABLE_TAG)
      {
         // Deterministic case - check whether cval has this class or not
         if (TAGOF(type) == COMPOUND_TAG)
         {
            // upper/1, lower/1, to_upper/1, to_lower/1
            if (FUNCTOROF(type) == upperFunctor)
            {
               return (isupper(cval) && unify(ARGOF(type, 0), MAKE_INTEGER(tolower(cval))));
            }
            else if (FUNCTOROF(type) == lowerFunctor)
            {
               return (islower(cval) && unify(ARGOF(type, 0), MAKE_INTEGER(toupper(cval))));
            }
            else if (FUNCTOROF(type) == toLowerFunctor)
            {
               return unify(ARGOF(type, 0), MAKE_INTEGER(toupper(cval)));
            }
            else if (FUNCTOROF(type) == toUpperFunctor)
            {
               return unify(ARGOF(type, 0), MAKE_INTEGER(tolower(cval)));
            }
            else if (FUNCTOROF(type) == xdigitFunctor)
            {
               if (isxdigit(cval))
               {
                  if (cval >= 'a')
                     return unify(ARGOF(type, 0), MAKE_INTEGER(cval - 'a' + 10));
                  if (cval >= 'A')
                     return unify(ARGOF(type, 0), MAKE_INTEGER(cval - 'A' + 10));
                  return unify(ARGOF(type, 0), MAKE_INTEGER(cval - '0'));
               }
               return FAIL;
            }
            else if (FUNCTOROF(type) == digitFunctor)
            {
               return isdigit(cval) && unify(ARGOF(type, 0), MAKE_INTEGER(cval - '0'));
            }
            else if (FUNCTOROF(type) == parenFunctor)
            {
               if (cval == '(')
                  return unify(ARGOF(type, 0), MAKE_INTEGER(')'));
               if (cval == '{')
                  return unify(ARGOF(type, 0), MAKE_INTEGER('}'));
               if (cval == '[')
                  return unify(ARGOF(type, 0), MAKE_INTEGER(']'));
               return FAIL;
            }
            else
               return type_error(charTypeAtom, type);
         }
         else
         {
            if (type == alnumAtom)
               return isalnum(cval);
            else if (type == alphaAtom)
               return isalpha(cval);
            else if (type == asciiAtom)
               return cval <= 127;
            else if (type == cntrlAtom)
               return iscntrl(cval);
            else if (type == csymAtom)
               return isalnum(cval) || cval == '_';
            else if (type == csymfAtom)
               return isalpha(cval) || cval == '_';
            else if (type == digitAtom)
               return isdigit(cval);
            else if (type == endOfLineAtom)
               return cval >= 10 && cval <= 13;
            else if (type == graphAtom)
               return isgraph(cval);
            else if (type == lowerAtom)
               return islower(cval);
            else if (type == newlineAtom)
               return cval == 10;
            else if (type == periodAtom)
               return cval == 33 || cval == 46 || cval == 63;
            else if (type == prologAtomStartAtom)
               return islower(cval);
            else if (type == prologIdentifierContinueAtom)
               return isalnum(cval) || cval == '_';
            else if (type == prologSymbolAtom)
               return is_graphic_char(cval);
            else if (type == prologVarStartAtom)
               return isupper(cval) || cval == '_';
            else if (type == punctAtom)
               return ispunct(cval);
            else if (type == quoteAtom)
               return cval == 34 || cval == 39 || cval == 96;
            else if (type == spaceAtom)
               return isspace(cval);
            else if (type == upperAtom)
               return isupper(cval);
            else if (type == whiteAtom)
               return cval == 9 || cval == 32;
            return type_error(charTypeAtom, type);
         }
      }
      else
      {
         // Nondetermistic case - Find all classes for the code cval
         word list = backtrack;
         if (backtrack == 0)
         {
            // First call. Build a list of all the classes for cval
            List tmp;
            init_list(&tmp);
            if (isalnum(cval))
               list_append(&tmp, alnumAtom);
            if (isalpha(cval))
               list_append(&tmp, alphaAtom);
            if (cval <= 127)
               list_append(&tmp, asciiAtom);
            if (iscntrl(cval))
               list_append(&tmp, cntrlAtom);
            if (isalnum(cval) || cval == '_')
               list_append(&tmp, csymAtom);
            if (isalpha(cval) || cval == '_')
               list_append(&tmp, csymfAtom);
            if (isdigit(cval))
            {
               list_append(&tmp, digitAtom);
               list_append(&tmp, MAKE_VCOMPOUND(digitFunctor, MAKE_INTEGER(cval - '0')));
               list_append(&tmp, MAKE_VCOMPOUND(xdigitFunctor, MAKE_INTEGER(cval - '0')));
            }
            if (isxdigit(cval) && cval > '9')
            {
               int val = 0;
               if (cval >= 'a')
                  val = cval - 'a' + 10;
               else if (cval >= 'A')
                  val = cval - 'A' + 10;
               list_append(&tmp, MAKE_VCOMPOUND(xdigitFunctor, MAKE_INTEGER(val)));
            }
            if (cval >= 10 && cval <= 13)
               list_append(&tmp, endOfLineAtom);
            if (isgraph(cval))
               list_append(&tmp, graphAtom);
            if (islower(cval))
            {
               list_append(&tmp, lowerAtom);
               list_append(&tmp, MAKE_VCOMPOUND(lowerFunctor, MAKE_INTEGER(toupper(cval))));
            }
            if (isupper(cval))
            {
               list_append(&tmp, upperAtom);
               list_append(&tmp, MAKE_VCOMPOUND(upperFunctor, MAKE_INTEGER(tolower(cval))));
            }
            if (cval == 10)
               list_append(&tmp, newlineAtom);
            if (cval == 33 || cval == 46 || cval == 63)
               list_append(&tmp, periodAtom);
            if (islower(cval))
               list_append(&tmp, prologAtomStartAtom);
            if (isalnum(cval) || cval == '_')
               list_append(&tmp, prologIdentifierContinueAtom);
            if (is_graphic_char(cval))
               list_append(&tmp, prologSymbolAtom);
            if (isupper(cval) || cval == '_')
               list_append(&tmp, prologVarStartAtom);
            if (ispunct(cval))
               list_append(&tmp, punctAtom);
            if (cval == 34 || cval == 39 || cval == 96)
               list_append(&tmp, quoteAtom);
            if (isspace(cval))
               list_append(&tmp, spaceAtom);
            if (cval == 9 || cval == 32)
               list_append(&tmp, whiteAtom);
            if (cval == '(')
               list_append(&tmp, MAKE_VCOMPOUND(parenFunctor, MAKE_INTEGER(')')));
            if (cval == '{')
               list_append(&tmp, MAKE_VCOMPOUND(parenFunctor, MAKE_INTEGER('}')));
            if (cval == '[')
               list_append(&tmp, MAKE_VCOMPOUND(parenFunctor, MAKE_INTEGER(']')));
            list_append(&tmp, MAKE_VCOMPOUND(toLowerFunctor, MAKE_INTEGER(toupper(cval))));
            list_append(&tmp, MAKE_VCOMPOUND(toUpperFunctor, MAKE_INTEGER(tolower(cval))));
            list = term_from_list(&tmp, emptyListAtom);
            free_list(&tmp);
         }
         if (list == emptyListAtom)
            return FAIL;
         if (TAGOF(list) == COMPOUND_TAG && FUNCTOROF(list) == listFunctor)
            make_foreign_choicepoint(ARGOF(list,1));
         return unify(ARGOF(list, 0), type);
      }
   }
})

// non-iso
PREDICATE(succ, 2, (word a, word b)
{
   if (TAGOF(a) == VARIABLE_TAG)
   {
      if (!must_be_positive_integer(b))
         return ERROR;
      long b_val = getConstant(b, NULL).integer_data;
      if (b_val == 0)
         return FAIL;
      return unify(a, MAKE_INTEGER(b_val-1));
   }
   if (TAGOF(b) != VARIABLE_TAG && !must_be_positive_integer(b))
      return ERROR;
   if (!must_be_positive_integer(a))
      return ERROR;
   long a_val = getConstant(a, NULL).integer_data;
   return unify(b, MAKE_INTEGER(a_val+1));
})

// non-iso
PREDICATE(callable, 1, (word a)
{
   return (TAGOF(a) == CONSTANT_TAG && getConstantType(a) == ATOM_TYPE) || TAGOF(a) == COMPOUND_TAG;
})

// non-iso
PREDICATE(nb_setarg, 3, (word arg, word term, word value)
{
   value = DEREF(value);
   if (!must_be_bound(term))
      return ERROR;
   if (TAGOF(term) != COMPOUND_TAG)
      return type_error(compoundAtom, term);
   if (TAGOF(value) != CONSTANT_TAG)
      value = copy_term(value);
   if (!must_be_positive_integer(arg))
      return ERROR;
   long i = getConstant(arg, NULL).integer_data;
   *((Word)(((Word*)(DEREF(term) & ~TAG_MASK))+i)) = value;
   return SUCCESS;
})

// consult
PREDICATE(consult, 1, (word a)
{
   if (!must_be_atom(a))
      return ERROR;
   Stream s = fileReadStream(getConstant(a, NULL).atom_data->data);
   if (s == NULL)
   {
      existence_error(sourceSinkAtom, a);
      return ERROR;
   }
   consult_stream(s);
   s->close(s);
   freeStream(s);
   return SUCCESS;
})

PREDICATE(nth0, 3, (word n, word list, word item)
{
   int i;
   if (!must_be_positive_integer(n))
      return 0;
   long position = getConstant(n, NULL).integer_data;
   for (i = 0; i < position && TAGOF(list) == COMPOUND_TAG && FUNCTOROF(list) == listFunctor; i++)
      list = ARGOF(list, 1);
   return i == position && TAGOF(list) == COMPOUND_TAG && FUNCTOROF(list) == listFunctor && unify(item, ARGOF(list, 0));
})

PREDICATE(qqq, 0, ()
{
 qqq();
 return SUCCESS;
})

PREDICATE($heap_usage, 1, (word u)
{
   return unify(MAKE_INTEGER(heap_usage()), u);
})
