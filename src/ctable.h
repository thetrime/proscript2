#define ATOM_TYPE 1
#define INTEGER_TYPE 2
#define FUNCTOR_TYPE 3
#include "types.h"
#include "bihashmap.h"

word intern(int type, uint32_t hashcode, void* key1, int key2, void*(*create)(void*, int), int* isNew);
constant getConstant(word);
