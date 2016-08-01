#include "kernel.h"
#include "errors.h"
#include "stream.h"
#include "parser.h"
#include <stdio.h>

PREDICATE(=, 2, (word a, word b)
{
   return unify(a,b);
})

PREDICATE(var, 1, (word a)
{
   return TAGOF(a) == VARIABLE_TAG;
})

PREDICATE(atom, 1, (word a)
{
   return TAGOF(a) == CONSTANT_TAG && getConstant(a).type == ATOM_TYPE;
})

PREDICATE(integer, 1, (word a)
{
   return TAGOF(a) == CONSTANT_TAG && getConstant(a).type == INTEGER_TYPE;
})

PREDICATE(float, 1, (word a)
{
   return TAGOF(a) == CONSTANT_TAG && getConstant(a).type == FLOAT_TYPE;
})

PREDICATE(atomic, 1, (word a)
{
   return TAGOF(a) == CONSTANT_TAG;
})

PREDICATE(compound, 1, (word a)
{
   return TAGOF(a) == COMPOUND_TAG;
})

PREDICATE(nonvar, 1, (word a)
{
   return TAGOF(a) != VARIABLE_TAG;
})

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

PREDICATE(copy_term, 2, (word term, word copy)
{
   return unify(copy, copy_term(term));
})


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

PREDICATE(asserta, 1, (word term)
{
   assert(0);
})

PREDICATE(assertz, 1, (word term)
{
   assert(0);
})

NONDET_PREDICATE(asserta, 1, (word term, word backtrack)
{
   assert(0);
})

PREDICATE(abolish, 1, (word indicator)
{
   assert(0);
})

NONDET_PREDICATE(repeat, 0, (word backtrack)
{
   make_foreign_choicepoint(0);
   return SUCCESS;
})

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

NONDET_PREDICATE(sub_atom, 5, (word atom, word before, word length, word after, word subatom, word backtrack)
{
   assert(0);
})

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


