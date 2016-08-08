#include "constants.h"
#include "types.h"
#include "kernel.h"

int type_error(word expected, word actual);
int instantiation_error();
int existence_error(word type, word value);
int domain_error(word type, word value);
int permission_error(word operation, word type, word culprit);
int representation_error(word flag, word what);
int io_error(word what, word where);
int integer_overflow();
int float_overflow();
int zero_divisor();
int syntax_error(word message);
int format_error(word message);
