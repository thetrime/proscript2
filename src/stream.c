#include "stream.h"
#include "kernel.h"
#include "errors.h"
#include "constants.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

char get_raw_char(Stream s)
{
   return getch(s);
}
char peek_raw_char(Stream s)
{
   return peekch(s);
}

int flush_stream(Stream s)
{
   if (s->write == NULL)
   {
      permission_error(outputAtom, streamAtom, s->term);
      return -1;
   }
   int bytesAvailableToFlush = s->filled_buffer_size - s->buffer_ptr;
   if (bytesAvailableToFlush > 0)
   {
      int flushed =  s->write(s, bytesAvailableToFlush, s->buffer);
      if (flushed < 0)
         return 0;
      if (flushed == bytesAvailableToFlush)
      {
         s->buffer_ptr = 0;
         s->filled_buffer_size = 0;
         if (s->flush != NULL)
            return s->flush(s);
         return 1;
      }
      else
         assert(0 && "Failed to flush buffer");
    }
    return 1; // Nothing to write
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

int putch(Stream s, int c)
{
   if (s->filled_buffer_size < 0)
   {
      return io_error(writeAtom, s->term);
   }
   s->buffer[s->filled_buffer_size++] = c;
   if ((s->filled_buffer_size == STREAM_BUFFER_SIZE || ((s->flags & STREAM_BUFFER) == 0)))
      return flush_stream(s);
   return SUCCESS;
}

int putb(Stream s, char c)
{
   if (s->filled_buffer_size < 0)
   {
      return io_error(writeAtom, s->term);
   }
   s->buffer[s->filled_buffer_size++] = c;
   if ((s->filled_buffer_size == STREAM_BUFFER_SIZE || ((s->flags & STREAM_BUFFER) == 0)))
      return flush_stream(s);
   return SUCCESS;
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
   //free(sb->data);  This is the responsibility of the caller to free
   free(sb);
}

static int id = 0;

Stream allocStream(int(*read)(struct stream*, int, unsigned char*),
                   int(*write)(struct stream*, int, unsigned char*),
                   int(*seek)(struct stream*, int, int),
                   int(*close)(struct stream*),
                   int(*flush)(struct stream*),
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
   s->buffer_ptr = 0;
   s->filled_buffer_size = 0;
   s->id = id++;
   s->term = MAKE_BLOB("stream", &s->id);
   s->flags = 0;
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
                      NULL,
                      freeStringBuffer,
                      allocStringBuffer(data, length));
}

Stream nullStream()
{
   return allocStream(NULL,
                      NULL,
                      NULL,
                      NULL,
                      NULL,
                      NULL,
                      NULL,
                      NULL);
}

int console_write(Stream stream, int length, unsigned char* buffer)
{
   printf("%.*s", length, buffer);
   return length;
}

Stream consoleOuputStream()
{
   return allocStream(NULL,
                      console_write,
                      NULL,
                      NULL,
                      NULL,
                      NULL,
                      NULL,
                      NULL);
}

int file_read(Stream stream, int length, unsigned char* buffer)
{
   return fread(buffer, sizeof(unsigned char), length, ((FILE*)stream->data));
}

int file_close(Stream stream)
{
   return fclose(((FILE*)stream->data));
}

size_t file_tell(Stream stream)
{
   return ftell(((FILE*)stream->data));
}

int file_flush(Stream stream)
{
   return fflush(((FILE*)stream->data));
}

int file_write(Stream stream, int length, unsigned char* buffer)
{
   return fwrite(buffer, sizeof(unsigned char), length, ((FILE*)stream->data));
}

void file_free(void* ignored)
{
}

int file_seek(Stream stream, int offset, int origin)
{
   return fseek(((FILE*)stream->data), offset, origin);
}



Stream fileReadStream(char* filename)
{
   FILE* fd = fopen(filename, "rb");
   assert(fd != NULL);
   return allocStream(file_read,
                      NULL,
                      NULL,
                      file_close,
                      NULL,
                      NULL,
                      NULL,
                      fd);
}

Stream fileStream(char* filename, word io_mode, Options* options)
{
   if (io_mode == readAtom)
   {
      FILE* fd = fopen(filename, "rb");
      assert(fd != NULL);
      return allocStream(file_read,
                         NULL,
                         file_seek,
                         file_close,
                         NULL, // flush
                         file_tell,
                         file_free,
                         fd);
   }
   else if (io_mode == writeAtom)
   {
      FILE* fd = fopen(filename, "wb");
      assert(fd != NULL);
      return allocStream(NULL,
                         file_write,
                         file_seek,
                         file_close,
                         file_flush,
                         file_tell,
                         file_free,
                         fd);
   }
   else if (io_mode == appendAtom)
   {
      FILE* fd = fopen(filename, "ab");
      assert(fd != NULL);
      return allocStream(NULL,
                         file_write,
                         file_seek,
                         file_close,
                         file_flush,
                         file_tell,
                         file_free,
                         fd);
   }
   domain_error(ioModeAtom, io_mode);
   return NULL;
}


int get_stream_position(Stream stream, word property)
{
    if (stream->tell != NULL)
       return unify(MAKE_VCOMPOUND(positionFunctor, MAKE_INTEGER(stream->tell(stream) - (stream->filled_buffer_size - stream->buffer_ptr))), property);
    return FAIL;
}
int (*stream_properties[])(Stream, word) = {get_stream_position, NULL};
