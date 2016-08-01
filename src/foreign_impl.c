#include "kernel.h"
#include "errors.h"
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
