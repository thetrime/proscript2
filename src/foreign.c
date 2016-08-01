#include "kernel.h"
#include "ctable.h"
#include "module.h"
#include "checks.h"
#include <string.h>
#include <assert.h>

int atom_difference(Atom a, Atom b)
{
   int i;
   if (a->length < b->length)
      i = strncmp(a->data, b->data, a->length);
   else
      i = strncmp(a->data, b->data, b->length);
   if (i == 0)
   {
      // Strings are the same up to the length of the shortest one
      if (a->length == b->length)
         return 0;
      else if (a->length < b->length)
         return -1;
      return 1;
   }
   return i;
}

int term_difference(word a, word b)
{
   a = DEREF(a);
   b = DEREF(b);
   if (a == b)
      return 0;
   if (TAGOF(a) == VARIABLE_TAG)
   {
      if (TAGOF(b) == VARIABLE_TAG)
         return a - b;
      return -1;
   }
   if (TAGOF(a) == CONSTANT_TAG)
   {
      constant ca = getConstant(a);
      if (ca.type == FLOAT_TYPE)
      {
         if (TAGOF(b) == VARIABLE_TAG)
            return 1;
         else if (TAGOF(b) == CONSTANT_TAG)
         {
            constant cb = getConstant(b);
            if (cb.type == FLOAT_TYPE)
               return ca.data.float_data->data - cb.data.float_data->data;
         }
         return -1;
      }
      if (ca.type == INTEGER_TYPE)
      {
         if (TAGOF(b) == VARIABLE_TAG)
            return 1;
         else if (TAGOF(b) == CONSTANT_TAG)
         {
            constant cb = getConstant(b);
            if (cb.type == FLOAT_TYPE)
               return 1;
            else if (cb.type == INTEGER_TYPE)
               return ca.data.integer_data->data - cb.data.integer_data->data;
         }
         return -1;
      }
      if (ca.type == ATOM_TYPE)
      {
         if (TAGOF(b) == VARIABLE_TAG)
            return 1;
         else if (TAGOF(b) == CONSTANT_TAG)
         {
            constant cb = getConstant(b);
            if (cb.type == FLOAT_TYPE || cb.type == INTEGER_TYPE)
               return 1;
            else if (cb.type == ATOM_TYPE)
            {
               return atom_difference(ca.data.atom_data, cb.data.atom_data);
            }
            return -1;
         }
      }
      else
      {
         // FIXME: bignum
         return -1;
      }
   }
   if (TAGOF(a) == COMPOUND_TAG)
   {
      if (TAGOF(b) != COMPOUND_TAG)
         return 1;
      Functor fa = getConstant(FUNCTOROF(a)).data.functor_data;
      Functor fb = getConstant(FUNCTOROF(b)).data.functor_data;
      if (fa->arity != fb->arity)
         return fa->arity - fb->arity;
      if (fa->name != fb->name)
         return atom_difference(getConstant(fa->name).data.atom_data, getConstant(fb->name).data.atom_data);
      for (int i = 0; i < fa->arity; i++)
      {
         int d = term_difference(ARGOF(a, i), ARGOF(b, i));
         if (d != 0)
            return d;
      }
      return 0;
   }
   assert(0);
}


int build_predicate_list(void* list, word key, any_t ignored)
{
   list_append((List*)list, key);
   return MAP_OK;
}

#define TOKENPASTE(x, y) x ## y
#define TOKENPASTE2(x, y) TOKENPASTE(x, y)
#define PREDICATE(name, arity, body) static int TOKENPASTE2(PRED_, __LINE__) body
#define NONDET_PREDICATE(name, arity, body) static int TOKENPASTE2(PRED_, __LINE__) body
#include "foreign_impl.c"
#undef PREDICATE
#undef NONDET_PREDICATE

void initialize_foreign()
{
 Module m = find_module(MAKE_ATOM("user"));
#define PREDICATE(name, arity, body) define_foreign_predicate_c(m, MAKE_FUNCTOR(MAKE_ATOM(#name), arity), TOKENPASTE2(PRED_, __LINE__), 0);
#define NONDET_PREDICATE(name, arity, body) define_foreign_predicate_c(m, MAKE_FUNCTOR(MAKE_ATOM(#name), arity), TOKENPASTE2(PRED_, __LINE__), NON_DETERMINISTIC);
#include "foreign_impl.c"
#undef PREDICATE
#undef NONDET_PREDICATE
#undef TOKENPASTE
#undef TOKENPASTE2

}
