#ifndef _CHAR_BUFFER_H
#define _CHAR_BUFFER_H
struct CharCell
{
   struct CharCell* next;
   char data;
};

typedef struct CharCell CharCell;

typedef struct
{
   int length;
   CharCell* head;
   CharCell* tail;
} charbuffer;

typedef charbuffer* CharBuffer;



CharBuffer charBuffer();
void push_char(CharBuffer cb, int c);
int char_buffer_length(CharBuffer buffer);
void free_char_buffer(CharBuffer buffer);
char* finalize_char_buffer(CharBuffer buffer);

#endif
