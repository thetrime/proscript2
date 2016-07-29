#include "types.h"

#define FUNCTOR(a,b,c) word a##Functor;
#define ATOM(a,b) word a##Atom;
#include "constants"
#undef FUNCTOR
#undef ATOM

void initialize_constants();
