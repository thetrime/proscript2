#include "global.h"
#include "constants.h"
#include "kernel.h"

void initialize_constants()
{
#define FUNCTOR(a,b,c) a##Functor = MAKE_FUNCTOR(MAKE_ATOM(b), c);
#define ATOM(a,b) a##Atom = MAKE_ATOM(b);
#include "constants"
#undef FUNCTOR
#undef ATOM
}
