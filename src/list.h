#ifndef _LIST_H
#define _LIST_H

#include "types.h"

struct cell_t
{
   struct cell_t* next;
   struct cell_t* prev;
   word data;
};

struct list_t
{
   struct cell_t* head;
   struct cell_t* tail;
   int length;
};

typedef struct list_t List;

void init_list(List* list);
void free_list(List* list);
struct cell_t* list_append(List* list, word w);
struct cell_t* list_unshift(List* list, word w);
void list_splice(List* list, struct cell_t*);
void list_apply(List* list, void*, void (*fn)(word, void*));
void list_apply_reverse(List* list, void*, void (*fn)(word, void*));
int list_length(List* list);
int list_contains(List* list, word);
int list_index(List* list, word);
word term_from_list(List* list, word tail);
word curly_from_list(List* list);
word list_shift(List* list);
word list_pop(List* list);
word list_delete_first(List* list, word w);
word list_element(List* list, int index);
int populate_list_from_term(List* list, word w);
#endif
