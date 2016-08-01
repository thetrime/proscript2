#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "options.h"
#include "stream.h"

struct string_cell_t
{
   struct string_cell_t* next;
   unsigned char* data;
   int length;
};

typedef struct string_cell_t string_cell_t;

typedef struct
{
   struct string_cell_t* head;
   struct string_cell_t* tail;
   int length;
} string_builder;

typedef string_builder* StringBuilder;

StringBuilder stringBuilder()
{
   StringBuilder b = malloc(sizeof(string_builder));
   b->head = NULL;
   b->tail = NULL;
   b->length = 0;
   return b;
}

// Do not free things passed to append_string - they will be freed for you
void append_string(StringBuilder b, unsigned char* data, int length)
{
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
   b->tail->next = NULL;
   b->tail->data = data;
   b->tail->length = length;
   b->length += length;
}

void finalize_buffer(StringBuilder b, unsigned char** text, int* length)
{
   *length = b->length;
   *text = malloc(*length);
   string_cell_t* cell = b->head;
   int i = 0;
   while (cell != NULL)
   {
      memcpy(*text + i, cell->data, cell->length);
      string_cell_t* tmp = cell->next;
      free(cell->data);
      free(cell);
      cell = tmp;
   }
   free(b);
}

StringBuilder format_term(Options* options, int precedence, word term)
{
   assert(0);
}

int write_term(Stream stream, word term, Options* options)
{
   StringBuilder formatted = format_term(options, 1200, term);
   unsigned char* text;
   int length;
   finalize_buffer(formatted, &text, &length);
   int rc = stream->write(stream, length, text) >= 0;
   free(text);
   return rc;
}

