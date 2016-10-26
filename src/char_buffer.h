#ifndef _CHAR_BUFFER_H
#define _CHAR_BUFFER_H
struct CharCell
{
   struct CharCell* next;
   int data;
};

typedef struct CharCell CharCell;

typedef struct
{
   int length;
   CharCell* head;
   CharCell* tail;
   int encoding;
} charbuffer;

typedef charbuffer* CharBuffer;



CharBuffer charBuffer();
void push_char(CharBuffer cb, int c);
int char_buffer_length(CharBuffer buffer);
void free_char_buffer(CharBuffer buffer);

typedef struct
{
   char encoding;
   union
   {
      char data[0];
      int ucs_data[0];
   };
} char_union;

char_union* finalize_char_buffer(CharBuffer buffer);

#endif
