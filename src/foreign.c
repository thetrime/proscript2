#include "global.h"
#include "kernel.h"
#include "ctable.h"
#include "module.h"
#include "checks.h"
#include "format.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>

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
      int ta;
      cdata ca = getConstant(a, &ta);
      if (ta == FLOAT_TYPE)
      {
         if (TAGOF(b) == VARIABLE_TAG)
            return 1;
         else if (TAGOF(b) == CONSTANT_TAG)
         {
            int tb;
            cdata cb = getConstant(b, &tb);
            if (tb == FLOAT_TYPE)
               return ca.float_data->data - cb.float_data->data;
         }
         return -1;
      }
      if (ta == INTEGER_TYPE)
      {
         if (TAGOF(b) == VARIABLE_TAG)
            return 1;
         else if (TAGOF(b) == CONSTANT_TAG)
         {
            int tb;
            cdata cb = getConstant(b, &tb);
            if (tb == FLOAT_TYPE)
               return 1;
            else if (tb == INTEGER_TYPE)
               return ca.integer_data - cb.integer_data;
         }
         return -1;
      }
      if (ta == ATOM_TYPE)
      {
         if (TAGOF(b) == VARIABLE_TAG)
            return 1;
         else if (TAGOF(b) == CONSTANT_TAG)
         {
            int tb;
            cdata cb = getConstant(b, &tb);
            if (tb == FLOAT_TYPE || tb == INTEGER_TYPE)
               return 1;
            else if (tb == ATOM_TYPE)
               return atom_difference(ca.atom_data, cb.atom_data);
         }
         return -1;
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
      Functor fa = getConstant(FUNCTOROF(a), NULL).functor_data;
      Functor fb = getConstant(FUNCTOROF(b), NULL).functor_data;
      if (fa->arity != fb->arity)
         return fa->arity - fb->arity;
      if (fa->name != fb->name)
         return atom_difference(getConstant(fa->name, NULL).atom_data, getConstant(fb->name, NULL).atom_data);
      for (int i = 0; i < fa->arity; i++)
      {
         int d = term_difference(ARGOF(a, i), ARGOF(b, i));
         if (d != 0)
            return d;
      }
      return 0;
   }
   if (TAGOF(a) == POINTER_TAG)
   {
      if (TAGOF(b) != POINTER_TAG)
         return -1;
      return GET_POINTER(a) - GET_POINTER(b);
   }
   printf(":: "); PORTRAY(a); printf("\n");
   assert(0 && "Illegal tag");
}

int acyclic_term(word t)
{
   List stack;
   List visited;
   init_list(&stack);
   init_list(&visited);
   list_append(&stack, t);
   while(list_length(&stack) != 0)
   {
      word arg = DEREF(list_pop(&stack));
      if (TAGOF(arg) == VARIABLE_TAG)
      {
         if (list_index(&visited, arg) != -1)
         {
            free_list(&visited);
            free_list(&stack);
            return 0;
         }
      }
      else if (TAGOF(arg) == COMPOUND_TAG)
      {
         list_append(&visited, arg);
         Functor f = getConstant(FUNCTOROF(arg), NULL).functor_data;
         for (int i = 0; i < f->arity; i++)
            list_append(&stack, ARGOF(arg, i));
      }
   }
   free_list(&visited);
   free_list(&stack);
   return 1;
}

Stream get_stream(word w)
{
   if (!must_be_bound(w))
      return NULL;
   if (TAGOF(w) == CONSTANT_TAG)
   {
      int type;
      cdata c = getConstant(w, &type);
      if (type == BLOB_TYPE && strcmp(c.blob_data->type, "stream") == 0)
      {
         return c.blob_data->ptr;
      }
      else if (type == ATOM_TYPE)
      {
         if (w == currentOutputAtom)
            return current_output;
         if (w == userErrorAtom)
            return current_output;
         // General purpose aliases not implemented yet. To do this we would have to be able to enumerate
         // all streams, or at least keep track of the ones with aliases
         domain_error(streamOrAliasAtom, w);
         return NULL;
      }
   }
   SET_EXCEPTION(domain_error(streamOrAliasAtom, w));
   return NULL;
}

int build_predicate_list(void* list, word key, any_t ignored)
{
   list_append((List*)list, key);
   return MAP_OK;
}

void strip_module(word term, word* clause, Module* module)
{
   if (TAGOF(term) == COMPOUND_TAG && FUNCTOROF(term) == crossModuleCallFunctor)
   {
      *module = find_module(ARGOF(term, 0));
      if (*module == NULL)
         *module = create_module(ARGOF(term, 0));
      *clause = ARGOF(term, 1);
   }
   else if (TAGOF(term) == COMPOUND_TAG && FUNCTOROF(term) == clauseFunctor)
   {
      word head = ARGOF(term, 0);
      if (TAGOF(head) == COMPOUND_TAG && FUNCTOROF(head) == crossModuleCallFunctor)
      {
         *module = find_module(ARGOF(term, 0));
         if (*module == NULL)
            *module = create_module(ARGOF(term, 0));
         *clause = MAKE_VCOMPOUND(clauseFunctor, ARGOF(head, 1), ARGOF(term, 1));
      }
      else
      {
         *module = get_current_module();
         *clause = term;
      }
   }
   else
   {
      *module = get_current_module();
      *clause = term;
   }
}

void PRETTY_PORTRAY(word term)
{
   Options _options;
   init_options(&_options);
   set_option(&_options, numbervarsAtom, trueAtom);
   set_option(&_options, quotedAtom, trueAtom);
   int rc = write_term(current_output, term, &_options);
   free_options(&_options);
}


typedef struct
{
   word value;
   int original_index;
} qkey_t;

int qcompare_keys(const void * a, const void* b)
{
   int d = term_difference(ARGOF(((qkey_t*)a)->value, 0), ARGOF(((qkey_t*)b)->value, 0));
   if (d == 0)
      return ((qkey_t*)a)->original_index - ((qkey_t*)b)->original_index;
   return d;
}

int qcompare_terms(const void * a, const void* b)
{
   return term_difference(*((word*)a), *((word*)b));
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
