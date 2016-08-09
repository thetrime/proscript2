#ifdef EMSCRIPTEN
#include <emscripten/emscripten.h>
#include <assert.h>

#include "types.h"
#include "kernel.h"
#include "ctable.h"

EMSCRIPTEN_KEEPALIVE
int atom_length(word a)
{
   assert(TAGOF(a) == CONSTANT_TAG);
   constant c = getConstant(a);
   assert(c.type == ATOM_TYPE);
   return c.data.atom_data->length;
}

EMSCRIPTEN_KEEPALIVE
char* atom_data(word a)
{
   assert(TAGOF(a) == CONSTANT_TAG);
   constant c = getConstant(a);
   assert(c.type == ATOM_TYPE);
   return c.data.atom_data->data;
}

EMSCRIPTEN_KEEPALIVE
long integer_data(word a)
{
   assert(TAGOF(a) == CONSTANT_TAG);
   constant c = getConstant(a);
   assert(c.type == INTEGER_TYPE);
   return c.data.integer_data->data;
}

EMSCRIPTEN_KEEPALIVE
double float_data(word a)
{
   assert(TAGOF(a) == CONSTANT_TAG);
   constant c = getConstant(a);
   assert(c.type == FLOAT_TYPE);
   return c.data.float_data->data;
}

EMSCRIPTEN_KEEPALIVE
int term_type(word a)
{
   if (TAGOF(a) == CONSTANT_TAG)
      return getConstant(a).type;
   else if (TAGOF(a) == VARIABLE_TAG)
      return 127;
   else if (TAGOF(a) == COMPOUND_TAG)
      return 128;
   else if (TAGOF(a) == POINTER_TAG)
      return 129;
   return -1;
}

EMSCRIPTEN_KEEPALIVE
word term_functor_name(word a)
{
   assert(TAGOF(a) == COMPOUND_TAG);
   return getConstant(FUNCTOROF(a)).data.functor_data->name;
}

EMSCRIPTEN_KEEPALIVE
int term_functor_arity(word a)
{
   assert(TAGOF(a) == COMPOUND_TAG);
   return getConstant(FUNCTOROF(a)).data.functor_data->arity;
}

EMSCRIPTEN_KEEPALIVE
word term_arg(word a, int i)
{
   assert(TAGOF(a) == COMPOUND_TAG);
   return ARGOF(a, i);
}

word make_atom(char* a, int l)
{
   return MAKE_NATOM(a, l);
}

#endif
