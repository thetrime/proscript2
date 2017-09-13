#include "global.h"
#include <stdlib.h>
#include "char_buffer.h"

CharBuffer charBuffer()
{
   CharBuffer cb = malloc(sizeof(charbuffer));
   cb->head = NULL;
   cb->tail = NULL;
   cb->length = 0;
   return cb;
}

void push_char(CharBuffer cb, int c)
{
   cb->length++;
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

void push_code(CharBuffer cb, int code)
{
   if (code <= 0x7f)
      push_char(cb, code);
   else if (code <= 0x800)
   {
      push_char(cb, (code >> 6) | 0xc0);
      push_char(cb, (code & 0x3f) | 0x80);
   }
   else if (code < 0xffff)
   {
      push_char(cb, (code >> 12) | 0xe0);
      push_char(cb, ((code >> 6) & 0x3f) | 0x80);
      push_char(cb, (code & 0x3f) | 0x80);
   }
   else
   {
      push_char(cb, 0xf0 | (code >> 18));
      push_char(cb, 0x80 | ((code >> 12) & 0x3f));
      push_char(cb, 0x80 | ((code >> 6) & 0x3f));
      push_char(cb, 0x80 | ((code & 0x3f)));
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


char* finalize_char_buffer(CharBuffer buffer)
{
   CharCell* c = buffer->head;
   char* data = malloc(buffer->length + 1);
   c = buffer->head;
   int i = 0;
   CharCell* d;
   while (c != NULL)
   {
      data[i++] = c->data;
      d = c->next;
      free(c);
      c = d;
   }
   data[i++] = '\0';
   free(buffer);
   return data;
}
