#include "constants.h"
#include "kernel.h"

void initialize_constants()
{
   errorFunctor = MAKE_FUNCTOR(MAKE_ATOM("error"), 2);
   crossModuleCallFunctor = MAKE_FUNCTOR(MAKE_ATOM(":"), 2);
   instantiationErrorAtom = MAKE_ATOM("instantiation_error");
   cutAtom = MAKE_ATOM("!");
   endOfFileAtom = MAKE_ATOM("end_of_file");
}
