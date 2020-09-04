#ifndef _TYPES_H
#define _TYPES_H
#include <stdlib.h>
#include "whashmap.h"
#include "list.h"
#include "options.h"
#include <gmp.h>



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
   int slot_count;
};
typedef struct clause clause;
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

typedef struct
{
   mpz_t data;
} biginteger;

typedef biginteger* BigInteger;
typedef struct
{
   mpq_t data;
} rational;

typedef rational* Rational;

struct options_t;

typedef struct
{
   void* ptr;
   char* type;
   char* (*portray)(char*, void*, struct options_t*, int, int*);
} blob;

typedef blob* Blob;


typedef struct
{
   double data;
} _float;

typedef _float* Float;

typedef union
{
   Atom atom_data;
   long integer_data;
   Functor functor_data;
   Float float_data;
   Blob blob_data;
   Rational rational_data;
   BigInteger biginteger_data;
   int tombstone_data;
} cdata;

struct constant
{
   int type;
   cdata data;
};
typedef struct constant constant;
typedef constant* Constant;


typedef enum
{
   Head,
   Body
} ChoicepointType;

struct choicepoint
{
   unsigned char* PC;
   struct frame* FR;
   struct frame* NFR;
   word* SP;
   word* TR;
   word* H;
   ChoicepointType type;
   struct choicepoint* CP;
   word functor;
   Clause clause;
   Module module;
   struct frame* cleanup;
   struct
   {
      void (*fn)(int, word);
      int arg;
   } foreign_cleanup;
   int argc;
   word args[0];
};
typedef struct choicepoint choicepoint;
typedef choicepoint* Choicepoint;

struct state
{
   Choicepoint choicepoint;
   int constant_state;
};
typedef struct state state;
typedef state* State;

struct frame
{
   struct frame* parent;
   int depth;
   Clause clause;
   int is_local;            // If 1 then we must call free_clause() on the clause after we cannot backtrack here again
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
