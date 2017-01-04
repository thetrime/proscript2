#ifndef _STREAM_H
#define _STREAM_H
#include <stddef.h>
#include "types.h"
#include "options.h"

#define STREAM_BUFFER_SIZE 1024

#define STREAM_BUFFER 1

struct stream
{
   int(*read)(struct stream*, int, unsigned char*);
   int(*write)(struct stream*, int, unsigned char*);
   int(*seek)(struct stream*, int, int);
   int(*close)(struct stream*);
   size_t (*tell)(struct stream*);
   int (*flush)(struct stream*);
   void(*free_stream)(void*);
   void* data;
   int flags;
   int filled_buffer_size;
   int buffer_ptr;
   word term;
   int id;
   unsigned char buffer[STREAM_BUFFER_SIZE];
};

typedef struct stream* Stream;

char get_raw_char(Stream s);
char peek_raw_char(Stream s);
int _getch(Stream s);
int peekch(Stream s);
int getb(Stream s);
int peekb(Stream s);
int putch(Stream s, int i);
int putb(Stream s, char i);
int flush(Stream s);
Stream stringBufferStream(char* data, int length);
Stream fileReadStream(char* filename);
Stream nullStream();
Stream fileStream(char* source_sink, word io_mode, Options* options);
Stream consoleOuputStream();
void freeStream(Stream s);

extern int (*stream_properties[])(Stream, word);

#endif

