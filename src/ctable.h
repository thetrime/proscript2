#define ATOM_TYPE 1
#define INTEGER_TYPE 2
#define FUNCTOR_TYPE 3
#include "types.h"

word intern(int type, Constant value, uint32_t hashcode, int(*compare)(Constant, Constant), int* isNew);
constant getConstant(word);
