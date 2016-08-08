#include "list.h"
#include "kernel.h"
#include "errors.h"
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

void list_splice(List* list, struct cell_t* cell)
{
   if (cell->prev != NULL)
      cell->prev->next = cell->next;
   else
      list->head = cell->next;
   if (cell->next != NULL)
      cell->next->prev = cell->prev;
   else
      list->tail = cell->prev;
   list->length--;
}

void list_delete_first(List* list, word w)
{
   struct cell_t* cell = list->head;
   while (cell != NULL)
   {
      // FIXME: This actually generates trailing which we dont really need
      word* local;
      word copy = copy_local(cell->data, &local);
      if (unify(copy, w))
      {
         list_splice(list, cell);
         free(local);
         return;
      }
      free(local);
      cell = cell->next;
   }
}

struct cell_t* list_append(List* list, word w)
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
   return list->tail;
}

struct cell_t* list_unshift(List* list, word w)
{
   if (list->head == NULL)
   {
      list->head = malloc(sizeof(struct cell_t));
      list->tail = list->head;
      list->head->next = NULL;
      list->head->prev = NULL;
   }
   else
   {
      struct cell_t* old_head = list->head;
      list->head = malloc(sizeof(struct cell_t));
      list->head->next = old_head;
      old_head->prev = list->head;
   }
   list->head->prev = NULL;
   list->head->data = w;
   list->length++;
   return list->head;
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
//   printf("CONSING %08lx\n", DEREF(cell));
   *r = MAKE_VCOMPOUND(listFunctor, DEREF(cell), *r);
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
   list->length--;
   return result;
}

word list_pop(List* list)
{
   word result = list->tail->data;
   struct cell_t* new_tail = list->tail->prev;
   if (new_tail == NULL) // List was a singleton. Tail and head are now null
      list->head = NULL;
   else if (new_tail->prev == NULL) // List had 2 items. Tail and head are the same and the list is a singleton
      list->head = new_tail;
   else
      new_tail->prev->next = new_tail; // General case.
   free(list->tail);
   list->tail = new_tail;
   list->length--;
   return result;
}


int populate_list_from_term(List* list, word w)
{
   word l = w;
   while (TAGOF(l) == COMPOUND_TAG && FUNCTOROF(l) == listFunctor)
   {
      list_append(list, ARGOF(l, 0));
      l = ARGOF(l, 1);
   }
   return (l == emptyListAtom) || type_error(listAtom, w);
}
