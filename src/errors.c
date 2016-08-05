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

int domain_error(word domain, word value)
{
   SET_EXCEPTION(MAKE_VCOMPOUND(errorFunctor, MAKE_VCOMPOUND(domainErrorFunctor, domain, value), MAKE_VAR()));
   return 0;
}

int permission_error(word operation, word type, word culprit)
{
 SET_EXCEPTION(MAKE_VCOMPOUND(errorFunctor, MAKE_VCOMPOUND(permissionErrorFunctor, operation, type, culprit), MAKE_VAR()));
   return 0;
}

int representation_error(word flag, word what)
{
   SET_EXCEPTION(MAKE_VCOMPOUND(errorFunctor, MAKE_VCOMPOUND(representationErrorFunctor, flag), MAKE_VAR()));
   return 0;
}

int integer_overflow()
{
   SET_EXCEPTION(MAKE_VCOMPOUND(errorFunctor, integerOverflowAtom, MAKE_VAR()));
   return 0;
}

int float_overflow()
{
   SET_EXCEPTION(MAKE_VCOMPOUND(errorFunctor, floatOverflowAtom, MAKE_VAR()));
   return 0;
}

int zero_divisor()
{
   SET_EXCEPTION(MAKE_VCOMPOUND(errorFunctor, zeroDivisorAtom, MAKE_VAR()));
   return 0;
}


int io_error(word what, word where) // Non-ISO
{
   SET_EXCEPTION(MAKE_VCOMPOUND(errorFunctor, MAKE_VCOMPOUND(ioErrorFunctor, what, where), MAKE_VAR()));
   return 0;
}

int syntax_error(word message) // Non-ISO
{
   SET_EXCEPTION(MAKE_VCOMPOUND(errorFunctor, MAKE_VCOMPOUND(syntaxErrorFunctor, message), MAKE_VAR()));
   return 0;

}
