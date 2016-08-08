#include <stdlib.h>
#include "whashmap.h"
#include "list.h"
#include <gmp.h>

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
      Rational rational_data;
      BigInteger biginteger_data;
      // ...
   } data;
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
   uintptr_t TR;
   word* H;
   ChoicepointType type;
   struct choicepoint* CP;
   word functor;
   Clause clause;
   Module module;
   struct frame* cleanup;
};
typedef struct choicepoint choicepoint;
typedef choicepoint* Choicepoint;

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
