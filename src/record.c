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



word make_dbref(word t, word** local, word** slot)
{
   word w = copy_local_with_extra_space(t, local, 3);
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
      word cx = make_dbref(term, &ptr, &slot);
      cell = list_unshift(list, cx);
   }
   else
   {
      list = malloc(sizeof(List));
      init_list(list);
      whashmap_put(database, key, list);
      cell = list_unshift(list, make_dbref(term, &ptr, &slot));
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
      cell = list_append(list, make_dbref(term, &ptr, &slot));
   else
   {
      list = malloc(sizeof(List));
      init_list(list);
      whashmap_put(database, key, list);
      cell = list_append(list, make_dbref(term, &ptr, &slot));
   }
   *slot = intern_record(list, key, cell, ptr);
   return *slot;
}

int erase(word ref)
{
   struct record_t* record = (struct record_t*)GET_POINTER(ref);
   list_splice(record->list, record->cell);
   if (record->local != NULL)
   {
      free(record->local);
   }
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
