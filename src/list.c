#include "list.h"
#include "kernel.h"
#include "constants.h"

void init_list(List* list)
{
   list->head = NULL;
   list->tail = NULL;
   list->length = 0;
}


void free_list(List* list)
{
   struct cell_t* cell = list->head;
   while (cell != NULL)
   {
      struct cell_t* next = cell->next;
      free(cell);
      cell = next;
   }
}

void list_append(List* list, word w)
{
   if (list->tail == NULL)
   {
      list->tail = malloc(sizeof(struct cell_t));
      list->head = list->tail;
      list->tail->prev = NULL;
   }
   else
   {
      list->tail->next = malloc(sizeof(struct cell_t));
      list->tail->next->prev = list->tail;
      list->tail = list->tail->next;
   }
   list->tail->next = NULL;
   list->tail->data = w;
   list->length++;
}

void list_apply(List* list, void* data, void (*fn)(word, void*))
{
   struct cell_t* cell = list->head;
   while (cell != NULL)
   {
      struct cell_t* next = cell->next;
      fn(cell->data, data);
      cell = next;
   }
}

void list_apply_reverse(List* list, void* data, void (*fn)(word, void*))
{
   struct cell_t* cell = list->tail;
   while (cell != NULL)
   {
      struct cell_t* next = cell->prev;
      fn(cell->data, data);
      cell = next;
   }
}


int list_contains(List* list, word w)
{
   struct cell_t* cell = list->tail;
   while (cell != NULL)
   {
      struct cell_t* next = cell->prev;
      if (w == cell->data)
         return 1;
      cell = next;
   }
   return 0;
}

int list_index(List* list, word w)
{
   struct cell_t* cell = list->tail;
   int i = 0;
   while (cell != NULL)
   {
      struct cell_t* next = cell->prev;
      if (w == cell->data)
         return i;
      i++;
      cell = next;
   }
   return -1;
}


int list_length(List* list)
{
   return list->length;
}

void cons(word cell, void* result)
{
   word* r = (word*)result;
   *r = MAKE_VCOMPOUND(listFunctor, cell, *r);
}

word term_from_list(List* list, word tail)
{
   word result = tail;
   list_apply_reverse(list, &result, cons);
   return result;
}

word list_shift(List* list)
{
   word result = list->head->data;
   struct cell_t* new_head = list->head->next;
   if (new_head == NULL) // List was a singleton. Tail and head are now null
      list->tail = NULL;
   else if (new_head->next == NULL) // List had 2 items. Tail and head are the same and the list is a singleton
      list->tail = new_head;
   else
      new_head->next->prev = new_head; // General case.
   free(list->head);
   list->head = new_head;
   return result;
}

int populate_list_from_term(List* list, word w)
{
   while (TAGOF(w) == COMPOUND_TAG && FUNCTOROF(w) == listFunctor)
   {
      list_append(list, ARGOF(w, 0));
      w = ARGOF(w, 1);
   }
   return w == emptyListAtom;
}
