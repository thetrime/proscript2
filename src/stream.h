#ifndef _STREAM_H
#define _STREAM_H
#include <stddef.h>

#define STREAM_BUFFER_SIZE 1024

struct stream
{
   int(*read)(struct stream*, int, unsigned char*);
   int(*write)(struct stream*, int, unsigned char*);
   int(*seek)(struct stream*, int, int);
   int(*close)(struct stream*);
   size_t (*tell)(struct stream*);
   void(*free)(void*);
   void* data;
   int filled_buffer_size;
   int buffer_ptr;
   unsigned char buffer[STREAM_BUFFER_SIZE];
};

typedef struct stream* Stream;

char get_raw_char(Stream s);
char peek_raw_char(Stream s);
int getch(Stream s);
int peekch(Stream s);
int getb(Stream s);
int peekb(Stream s);

Stream stringBufferStream(char* data, int length);
void freeStream(Stream s);
#endif
