#include "types.h"

#undef ATOM
#undef FUNCTOR
#define FUNCTOR(a,b,c) extern word a##Functor;
#define ATOM(a,b) extern word a##Atom;
#include "constants"
#undef FUNCTOR
#undef ATOM

void initialize_constants();
