#include "types.h"
#include "kernel.h"
#include "ctable.h"
#include <string.h>

constant* CTable = NULL;
bimap_t map[4];
int CTableSize = 0;
int CNext = 0;


constant getConstant(word w)
{
   return CTable[w >> 2];
}


int atom_compare(void* data, int len, word w)
{
   Atom a = getConstant(w).data.atom_data;
   return(a->length == len && strncmp(data, a->data, len) == 0);
}

int integer_compare(void* data, int len, word w)
{
   Integer i = getConstant(w).data.integer_data;
   return i->data == *((long*)data);
}

int functor_compare(void* data, int len, word w)
{
   Functor f = getConstant(w).data.functor_data;
   return f->name == *((word*)data) && f->arity == len;
}

void initialize_ctable()
{
   CTableSize = 256;
   map[ATOM_TYPE] = bihashmap_new(atom_compare);
   map[INTEGER_TYPE] = bihashmap_new(integer_compare);
   map[FUNCTOR_TYPE] = bihashmap_new(functor_compare);
   CTable = calloc(CTableSize, sizeof(constant));
}

word intern(int type, uint32_t hashcode, void* key1, int key2, void*(*create)(void*, int), int* isNew)
{
   if (CTable == NULL)
      initialize_ctable();
   word w;
   if (bihashmap_get(map[type], hashcode, key1, key2, &w) == MAP_OK)
   {
      if (isNew != NULL)
         *isNew = 0;
      return w;
   }
   if (CTableSize == CNext)
   {
      CTableSize <<= 1;
      constant* c = calloc(CTableSize, sizeof(constant));
      memcpy(c, CTable, CNext * sizeof(constant));
      CTable = c;
   }
   w = (word)((CNext << 2) | CONSTANT_TAG);
   CTable[CNext].type = type;
   switch(type)
   {
      case ATOM_TYPE: CTable[CNext++].data.atom_data = (Atom)create(key1, key2); break;
      case FUNCTOR_TYPE: CTable[CNext++].data.functor_data = (Functor)create(key1, key2); break;
      case INTEGER_TYPE: CTable[CNext++].data.integer_data = (Integer)create(key1, key2); break;
   }

   if (isNew != NULL)
      *isNew = 1;
   bihashmap_put(map[type], hashcode, key1, key2, w);
   return w;
}
