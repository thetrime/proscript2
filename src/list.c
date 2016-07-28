#include "list.h"

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


int list_length(List* list)
{
   return list->length;
}
