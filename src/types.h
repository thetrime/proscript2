#include <stdlib.h>
#include "whashmap.h"

#ifndef _TYPES_H
typedef uintptr_t word;
typedef word* Word;


typedef struct
{
   word name;
   int arity;
} functor;
typedef functor* Functor;

typedef struct
{
   word name;
   wmap_t predicates;
} module;
typedef module* Module;

struct clause
{
   word* constants;
   uint8_t* code;
   struct clause* next;
   int code_size;
   int constant_size;
};
typedef struct clause clause;
typedef clause* Clause;

struct predicate_cell_t
{
   word term;
   struct predicate_cell_t* next;
};

typedef struct predicate_cell_t predicate_cell_t;

typedef struct
{
   predicate_cell_t* head;
   predicate_cell_t** tail;
   Clause firstClause;
   char* meta;
   int flags;
} predicate;

typedef predicate* Predicate;

typedef struct
{
   int variable_count;
   word* variables;
   Clause clause;
} query;
typedef query* Query;

typedef struct
{
   char* data;
   int length;
} atom;

typedef atom* Atom;

typedef struct
{
   long data;
} integer;

typedef integer* Integer;

typedef struct
{
   void* ptr;
   char* type;
} blob;

typedef blob* Blob;


typedef struct
{
   double data;
} _float;

typedef _float* Float;


struct constant
{
   int type;
   union
   {
      Atom atom_data;
      Integer integer_data;
      Functor functor_data;
      Float float_data;
      Blob blob_data;
      // ...
   } data;
};
typedef struct constant constant;
typedef constant* Constant;

typedef struct
{
   void* foreign;
   word catcher;
} cleanup;

typedef cleanup* Cleanup;

struct choicepoint
{
   unsigned char* PC;
   struct frame* FR;
   struct frame* NFR;
   uintptr_t SP;
   uintptr_t TR;
   int H;
   struct choicepoint* CP;
   word functor;
   Clause clause;
   Module module;
   Cleanup cleanup;
};
typedef struct choicepoint choicepoint;
typedef choicepoint* Choicepoint;

struct frame
{
   struct frame* parent;
   int depth;
   Clause clause;
   Module contextModule;
   unsigned char* returnPC;
   Choicepoint choicepoint;
   word functor;
   word slots[0];
};
typedef struct frame frame;
typedef frame* Frame;
#define _TYPES_H
#endif
