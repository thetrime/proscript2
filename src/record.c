#include "types.h"
#include "record.h"
#include "whashmap.h"
#include "list.h"
#include "ctable.h"
#include "constants.h"
#include "kernel.h"
#include "compiler.h"
#include <assert.h>
#include <stdio.h>

wmap_t database;

struct record_t
{
   List* list;
   struct cell_t* cell;
   word* local;
   word key;
};


void initialize_database()
{
   database = whashmap_new();
}

int count_compounds(word t)
{
   t = DEREF(t);
   if (TAGOF(t) == COMPOUND_TAG)
   {
      int i = 1;
      Functor f = getConstant(FUNCTOROF(t)).data.functor_data;
      for (int j = 0; j < f->arity; j++)
         i+=count_compounds(ARGOF(t, j));
      return i + f->arity;
   }
   return 0;
}

word denature_term_(word t, List* variables, word* heap, int* ptr)
{
   t = DEREF(t);
   switch(TAGOF(t))
   {
      case CONSTANT_TAG:
         return t;
      case POINTER_TAG:
         return t;
      case COMPOUND_TAG:
      {
         word result = (word)(&heap[*ptr]) | COMPOUND_TAG;
         heap[*ptr] = FUNCTOROF(t);
         (*ptr)++;
         int argp = *ptr;
         Functor f = getConstant(FUNCTOROF(t)).data.functor_data;
         (*ptr) += f->arity;
         for (int i = 0; i < f->arity; i++)
         {
            heap[argp++] = denature_term_(ARGOF(t, i), variables, heap, ptr);
         }
         return result;
      }
      case VARIABLE_TAG:
      {
         int i = list_index(variables, t);
         heap[i+3] = (word)&heap[i+3];
         return ((word)heap)+i+3;
      }
   }
   assert(0);
}

// Produce a thing which is like a term but is not on the heap
// FIXME: we need to clean up this local storage at some point!
word denature_term(word t, word** local, word** slot)
{
   List variables;
   init_list(&variables);
   int i = count_compounds(t);
   find_variables(t, &variables);
   i+= list_length(&variables);
   i+=3; // Save space for the ref itself
   *local = malloc(sizeof(word) * i);
   int ptr = 3+list_length(&variables);
   word w = denature_term_(t, &variables, *local, &ptr);
   free_list(&variables);
   // Write the functor
   (*local)[0] = dbCellFunctor;
   (*local)[1] = w;
   // Make slot a variable for now so that the term isnt temporary garbage
   (*local)[2] = (word)&((*local)[2]);
   // Pass it out so we can fill it in later
   *slot = &((*local)[2]);
   return (word)(*local) | COMPOUND_TAG;
}


word intern_record(List* list, word key, struct cell_t* cell, word* ptr)
{
   struct record_t* record = malloc(sizeof(struct record_t));
   record->list = list;
   record->cell = cell;
   record->local = ptr;
   record->key = key;
   return MAKE_POINTER(record);
}

word recorda(word key, word term)
{
   List* list;
   struct cell_t* cell;
   word* ptr;
   word* slot;
   if (whashmap_get(database, key, (any_t)&list) == MAP_OK)
   {
      word cx = denature_term(term, &ptr, &slot);
      cell = list_unshift(list, cx);
   }
   else
   {
      list = malloc(sizeof(List));
      init_list(list);
      whashmap_put(database, key, list);
      cell = list_unshift(list, denature_term(term, &ptr, &slot));
   }
   *slot = intern_record(list, key, cell, ptr);
   return *slot;
}
word recordz(word key, word term)
{
   List* list;
   struct cell_t* cell;
   word* ptr;
   word* slot;
   if (whashmap_get(database, key, (any_t)&list) == MAP_OK)
      cell = list_append(list, denature_term(term, &ptr, &slot));
   else
   {
      list = malloc(sizeof(List));
      init_list(list);
      whashmap_put(database, key, list);
      cell = list_append(list, denature_term(term, &ptr, &slot));
   }
   *slot = intern_record(list, key, cell, ptr);
   return *slot;
}

int erase(word ref)
{
   struct record_t* record = (struct record_t*)GET_POINTER(ref);
   list_splice(record->list, record->cell);
   if (record->local != NULL)
      free(record->local);
   free(record);
   return SUCCESS;
}

int recorded(word ref, word* key, word* value)
{
   struct record_t* record = (struct record_t*)GET_POINTER(ref);
   *key = record->key;
   *value = record->cell->data;
   return 1;
}

word find_records(word key)
{
   List* list;
   if (whashmap_get(database, key, (any_t)&list) == MAP_OK)
      return term_from_list(list, emptyListAtom);
   return emptyListAtom;
}
