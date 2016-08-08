#ifndef _STRING_BUILDER_H
#define _STRING_BUILDER_H

#include "types.h"

struct string_cell_t
{
   struct string_cell_t* next;
   char* data;
   int length;
   int must_free;
};

typedef struct string_cell_t string_cell_t;

typedef struct
{
   struct string_cell_t* head;
   struct string_cell_t* tail;
   int length;
} string_builder;

typedef string_builder* StringBuilder;

StringBuilder stringBuilder();
void append_string_no_copy(StringBuilder b, char* data, int length);
void append_string(StringBuilder b, char* data, int length);
void append_atom(StringBuilder b, Atom a);
void finalize_buffer(StringBuilder b, char** text, int* length);
void freeStringBuilder(StringBuilder b);
int lastChar(StringBuilder b);
#endif
