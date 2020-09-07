#include "global.h"
#include "types.h"
#include "kernel.h"
#include "ctable.h"

#undef ATOM
#undef FUNCTOR
#define FUNCTOR(a,b,c) word a##Functor;
#define ATOM(a,b) word a##Atom;
#include "constants"
#undef FUNCTOR
#undef ATOM

void initialize_constants()
{
#undef ATOM
#undef FUNCTOR
#define FUNCTOR(a,b,c) a##Functor = acquire_constant(MAKE_FUNCTOR(MAKE_ATOM(b), c));
#define ATOM(a,b) a##Atom = acquire_constant(MAKE_ATOM(b));
#include "constants"
#undef FUNCTOR
#undef ATOM
}
