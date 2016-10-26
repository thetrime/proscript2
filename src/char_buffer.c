#include "global.h"
#include <stdlib.h>
#include "char_buffer.h"
#include "types.h"

CharBuffer charBuffer()
{
   CharBuffer cb = malloc(sizeof(charbuffer));
   cb->head = NULL;
   cb->tail = NULL;
   cb->length = 0;
   cb->encoding = ENCODING_ISO_LATIN_1;
   return cb;
}

void push_char(CharBuffer cb, int c)
{
   cb->length++;
   if (c > 0xff)
      cb->encoding = ENCODING_UCS;
   if (cb->tail == NULL)
   {
      cb->head = malloc(sizeof(CharCell));
      cb->tail = cb->head;
      cb->head->next = NULL;
      cb->head->data = c;
   }
   else
   {
      cb->tail->next = malloc(sizeof(CharCell));
      cb->tail = cb->tail->next;
      cb->tail->data = c;
      cb->tail->next = NULL;
   }
}

int char_buffer_length(CharBuffer buffer)
{
   return buffer->length;
}

void free_char_buffer(CharBuffer buffer)
{
   free(buffer);
}


char_union* finalize_char_buffer(CharBuffer buffer)
{
   CharCell* c = buffer->head;
   if (buffer->encoding == ENCODING_ISO_LATIN_1)
   {
      char_union* u = malloc(buffer->length + 1 + sizeof(char_union));
      c = buffer->head;
      int i = 0;
      CharCell* d;
      while (c != NULL)
      {
         u->data[i++] = c->data;
         d = c->next;
         free(c);
         c = d;
      }
      u->data[i++] = '\0';
      free(buffer);
      return u;
   }
   else
   {
      char_union* u = malloc(sizeof(int)* (buffer->length + 1) + sizeof(char_union));
      c = buffer->head;
      int i = 0;
      CharCell* d;
      while (c != NULL)
      {
         u->ucs_data[i++] = c->data;
         d = c->next;
         free(c);
         c = d;
      }
      u->ucs_data[i++] = '\0';
      free(buffer);
      return u;
   }
}
