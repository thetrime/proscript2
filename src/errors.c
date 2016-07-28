#include "errors.h"

int type_error(word expected, word actual)
{
   SET_EXCEPTION(MAKE_VCOMPOUND(errorFunctor, MAKE_VCOMPOUND(typeErrorFunctor, expected, actual), MAKE_VAR()));
   return 0;
}
