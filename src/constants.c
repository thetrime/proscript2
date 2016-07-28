#include "constants.h"
#include "kernel.h"

void initialize_constants()
{
   errorFunctor = MAKE_FUNCTOR(MAKE_ATOM("error"), 2);
   crossModuleCallFunctor = MAKE_FUNCTOR(MAKE_ATOM(":"), 2);
   listFunctor = MAKE_FUNCTOR(MAKE_ATOM("."), 2);
   curlyFunctor = MAKE_FUNCTOR(MAKE_ATOM("{}"), 1);
   conjunctionFunctor = MAKE_FUNCTOR(MAKE_ATOM(","), 2);

   instantiationErrorAtom = MAKE_ATOM("instantiation_error");
   cutAtom = MAKE_ATOM("!");
   endOfFileAtom = MAKE_ATOM("end_of_file");
   emptyListAtom = MAKE_ATOM("[]");
   curlyAtom = MAKE_ATOM("{}");
   falseAtom = MAKE_ATOM("false");
   trueAtom = MAKE_ATOM("true");
   codesAtom = MAKE_ATOM("codes");
   charsAtom = MAKE_ATOM("chars");
   atomAtom = MAKE_ATOM("atom");
   errorAtom = MAKE_ATOM("error");

}
