#include "stream.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char get_raw_char(Stream s)
{
   return getch(s);
}
char peek_raw_char(Stream s)
{
   return peekch(s);
}

int getch(Stream s)
{
   int b = getb(s);
   if (b == -1)
      return -1;
   else if (b <= 0x7f)
      return b;
   int ch = 0;
   int i = 0;
   for (int mask = 0x20; mask != 0; mask >>= 1)
   {
      int next = getb(s);
      if (next == -1)
         return -1;
      ch = (ch << 6) ^ (next & 0x3f);
      if ((b & mask) == 0)
         break;
      i++;
   }
   return ch ^ (b & (0xff >> (i+3))) << (6*(i+1));
}

int peekch(Stream s)
{
   int b = peekb(s);
   if (b == -1)
      return -1;
   else if (b <= 0x7f)
      return b;
   int ch = 0;
   int i = 0;
   for (int mask = 0x20; mask != 0; mask >>= 1)
   {
      if (i > s->filled_buffer_size)
      {
         printf("Fatal - break in unicode peek\n");
         return -1;
      }
      int next = s->buffer[i+1];
      if (next == -1)
         return -1;
      ch = (ch << 6) ^ (next & 0x3f);
      if ((b & mask) == 0)
         break;
      i++;
   }
   return ch ^ (b & (0xff >> (i+3))) << (6*(i+1));
}

int getb(Stream s)
{
   if (s->buffer_ptr == s->filled_buffer_size)
   {
      // must read
      s->buffer_ptr = 0;
      s->filled_buffer_size = s->read(s, STREAM_BUFFER_SIZE, s->buffer);
   }
   if (s->filled_buffer_size < 0)
      return s->filled_buffer_size;
   if (s->filled_buffer_size == 0)
      return -1;
   return s->buffer[s->buffer_ptr++];
}

int peekb(Stream s)
{
   if (s->buffer_ptr == s->filled_buffer_size)
   {
      // must read
      s->buffer_ptr = 0;
      s->filled_buffer_size = s->read(s, STREAM_BUFFER_SIZE, s->buffer);
   }
   if (s->filled_buffer_size < 0)
      return s->filled_buffer_size;
   if (s->filled_buffer_size == 0)
      return -1;
   return s->buffer[s->buffer_ptr];
}

typedef struct
{
   int length;
   char* data;
} stringbuffer;

typedef stringbuffer* StringBuffer;

int string_read(Stream stream, int length, unsigned char* buffer)
{
   int bytesToRead = length;
   StringBuffer sb = (StringBuffer)stream->data;
   if (bytesToRead > sb->length)
      bytesToRead = sb->length;
   memcpy(buffer, sb->data, bytesToRead);
   sb->data += bytesToRead;
   sb->length -= bytesToRead;
   return bytesToRead;
}

StringBuffer allocStringBuffer(char* data, int len)
{
   StringBuffer sb = malloc(sizeof(stringbuffer));
   sb->length = len;
   sb->data = data;
   return sb;
}

void freeStringBuffer(void* t)
{
   StringBuffer sb = t;
   free(sb->data);
   free(sb);
}

Stream allocStream(int(*read)(struct stream*, int, unsigned char*),
                   int(*write)(struct stream*, int, unsigned char*),
                   int(*seek)(struct stream*, int, int),
                   int(*close)(struct stream*),
                   size_t (*tell)(struct stream*),
                   void(*free)(void*),
                   void* data)
{
   Stream s = malloc(sizeof(struct stream));
   s->read = read;
   s->write = write;
   s->seek = seek;
   s->close = close;
   s->tell = tell;
   s->free = free;
   s->data = data;
   return s;
}

void freeStream(Stream s)
{
   if (s->free != NULL)
      s->free(s->data);
   free(s);
}

Stream stringBufferStream(char* data, int length)
{
   return allocStream(string_read,
                      NULL,
                      NULL,
                      NULL,
                      NULL,
                      freeStringBuffer,
                      allocStringBuffer(data, length));
}
