#include "types.h"
#include "kernel.h"

constant* CTable = NULL;
int CTableSize = 0;

void initialize_ctable()
{
   CTableSize = 256;
   CTable = calloc(CTableSize, sizeof(constant));
}

word intern(int type, uint32_t hashcode, int(*compare)(const char*, const char*), int* isNew)
{
   if (CTable == NULL)
      initialize_ctable();
   return 0;
}

constant getConstant(word w)
{
   return CTable[w & ~TAG_MASK];
}

