#include <stdlib.h>

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
   word term;
} module;
typedef module* Module;

typedef struct
{
   word* constants;
   uint8_t* code;
} clause;
typedef clause* Clause;


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

struct constant
{
   int type;
   union
   {
      Atom atom_data;
      Integer integer_data;
      Functor functor_data;
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
   int (*apply)();
   struct choicepoint* previous;
   Cleanup cleanup;
};
typedef struct choicepoint choicepoint;
typedef choicepoint* Choicepoint;

struct frame
{
   struct frame* parent;
   int depth;
   word* slots;
   word* reserved_slots;
   Clause clause;
   Module contextModule;
   unsigned char* returnPC;
   Choicepoint choicepoint;
   word functor;
};
typedef struct frame frame;
typedef frame* Frame;
#define _TYPES_H
#endif
