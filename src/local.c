#include "local.h"
#include "compiler.h"
#ifdef EMSCRIPTEN
#include <emscripten/emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif



static int count_compounds(word t)
{
   t = DEREF(t);
   if (TAGOF(t) == COMPOUND_TAG)
   {
      int i = 1;
      Functor f = getConstant(FUNCTOROF(t), NULL).functor_data;
      for (int j = 0; j < f->arity; j++)
         i+=count_compounds(ARGOF(t, j));
      return i + f->arity;
   }
   return 0;
}


void make_local_(word t, List* variables, word* heap, int* ptr, int size, word* target, int mark_constants)
{
   // This do-while construct is used to make the COMPOUND_TAG case right-recursive
   // This means that processing a list uses O(1) C-stack rather than O(n).
   do
   {
      t = DEREF(t);
      switch(TAGOF(t))
      {
         case CONSTANT_TAG:
            if (mark_constants)
               acquire_constant("constant in local copy", t);
         case POINTER_TAG:
            *target = t;
            return;
         case COMPOUND_TAG:
         {
            *target = (word)(&heap[*ptr]) | COMPOUND_TAG;
            heap[*ptr] = FUNCTOROF(t);
            (*ptr)++;
            int argp = *ptr;
            if (mark_constants)
               acquire_constant("functor in local copy", FUNCTOROF(t));
            Functor f = getConstant(FUNCTOROF(t), NULL).functor_data;
            (*ptr) += f->arity;
            for (int i = 0; i < f->arity-1; i++)
            {
               make_local_(ARGOF(t, i), variables, heap, ptr, size, &heap[argp], mark_constants);
               argp++;
            }
            t = ARGOF(t, f->arity-1);
            target = &heap[argp];
            continue;
            return;
         }
         case VARIABLE_TAG:
         {
            // The variables appear, backwards, at the end of the heap. This is so that
            // The term we want is also identifiable as (word)heap
            int i = list_index(variables, t);
            heap[size-i-1] = (word)&heap[size-i-1];
            *target = (word)&heap[size-i-1];
            return;
         }
      }
      assert(0);
   } while(1);
}

void free_local(word w)
{
   forall_term_constants(w, "constant in local copy", release_constant);
   free((void*)w);
}

word copy_local_with_extra_space(word t, word** local, int extra, int mark_constants)
{
   t = DEREF(t);
   // First, if extra is 0 we can do some simple optimisations:
   if (extra == 0)
   {
      if (TAGOF(t) == CONSTANT_TAG || TAGOF(t) == POINTER_TAG)
      {
         if (TAGOF(t) == CONSTANT_TAG && mark_constants)
            acquire_constant("simple constant in local copy", t);
         *local = (word*)t;
         return t;
      }
      if (TAGOF(t) == VARIABLE_TAG)
      {
         word* localptr = malloc(sizeof(word));
         localptr[0] = (word)&localptr[0];
         return (word)&localptr[0];
      }
      // For compounds we must do the normal complicated process
   }


   List variables;
   init_list(&variables);
   int i = count_compounds(t);
   find_variables(t, &variables);
   i += list_length(&variables);
   i += extra;
   i++;
   word* localptr = malloc(sizeof(word) * i);
   assert(local != NULL);
   //printf("Allocated buffer for "); PORTRAY(t); printf(" at %d\n", (int)localptr);
   *local = localptr;

   int ptr = extra + 1;
   word w;
   make_local_(t, &variables, localptr, &ptr, i, &w, mark_constants);
   localptr[extra] = w;
   free_list(&variables);
   return w;
}

EMSCRIPTEN_KEEPALIVE
word copy_local(word t, word** local)
{
   return copy_local_with_extra_space(t, local, 0, 1);
}

