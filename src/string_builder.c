#include "global.h"
#include "string_builder.h"
#include <stdlib.h>
#include <string.h>

StringBuilder stringBuilder()
{
   StringBuilder b = malloc(sizeof(string_builder));
   b->head = NULL;
   b->tail = NULL;
   b->length = 0;
   return b;
}

// Do not free things passed to append_string - they will be freed for you
void append_string_no_copy(StringBuilder b, char* data, int length)
{
   if (length == 0)
      return;
   if (b->head == NULL)
   {
      b->head = malloc(sizeof(string_cell_t));
      b->tail = b->head;
   }
   else
   {
      b->tail->next = malloc(sizeof(string_cell_t));
      b->tail = b->tail->next;
   }
   b->tail->must_free = 0;
   b->tail->next = NULL;
   b->tail->data = data;
   b->tail->length = length;
   b->length += length;
}

void append_string(StringBuilder b, char* data, int length)
{
   if (length == 0)
   {
      free(data);
      return;
   }
   if (b->head == NULL)
   {
      b->head = malloc(sizeof(string_cell_t));
      b->tail = b->head;
   }
   else
   {
      b->tail->next = malloc(sizeof(string_cell_t));
      b->tail = b->tail->next;
   }
   b->tail->must_free = 1;
   b->tail->next = NULL;
   b->tail->data = data;
   b->tail->length = length;
   b->length += length;
}

void append_atom(StringBuilder b, Atom a)
{
   append_string_no_copy(b, a->data, a->length);
}

int lastChar(StringBuilder b)
{
   string_cell_t* cell = b->head;
   int i = 0;
   while (cell != NULL)
   {
      if (cell->next == NULL && cell->length != 0)
         return cell->data[cell->length-1];
      cell = cell->next;
   }
   return -1;
}


void finalize_buffer(StringBuilder b, char** text, int* length)
{
   *length = b->length;
   *text = malloc(*length);
   string_cell_t* cell = b->head;
   int i = 0;
   while (cell != NULL)
   {
      memcpy(*text + i, cell->data, cell->length);
      i+= cell->length;
      string_cell_t* tmp = cell->next;
      if (cell->must_free)
         free(cell->data);
      free(cell);
      cell = tmp;
   }
   free(b);
}

void freeStringBuilder(StringBuilder b)
{
   string_cell_t* cell = b->head;
   int i = 0;
   while (cell != NULL)
   {
      string_cell_t* tmp = cell->next;
      if (cell->must_free)
         free(cell->data);
      free(cell);
      cell = tmp;
   }
   free(b);
}

int length(StringBuilder b)
{
   return b->length;
}
