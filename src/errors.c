#include "errors.h"

int type_error(word expected, word actual)
{
   SET_EXCEPTION(MAKE_VCOMPOUND(errorFunctor, MAKE_VCOMPOUND(typeErrorFunctor, expected, actual), MAKE_VAR()));
   return 0;
}

int instantiation_error()
{
   SET_EXCEPTION(MAKE_VCOMPOUND(errorFunctor, instantiationErrorAtom, MAKE_VAR()));
   return 0;
}

int existence_error(word type, word value)
{
   SET_EXCEPTION(MAKE_VCOMPOUND(errorFunctor, MAKE_VCOMPOUND(existenceErrorFunctor, type, value), MAKE_VAR()));
   return 0;
}
