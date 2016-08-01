#include "types.h"
#include "kernel.h"
#include "ctable.h"
#include <string.h>
#include <stdio.h>

constant* CTable = NULL;
bimap_t map[4];
int CTableSize = 0;
int CNext = 0;


constant getConstant(word w)
{
   return CTable[w >> 2];
}

int atom_compare1(void* data, word w)
{
   Atom a = getConstant(w).data.atom_data;
   Atom b = (Atom)data;
   return(a->length == b->length && strncmp(b->data, a->data, a->length) == 0);
}

int atom_compare2(void* data, int len, word w)
{
   Atom a = getConstant(w).data.atom_data;
   return(a->length == len && strncmp(data, a->data, len) == 0);
}

int integer_compare1(void* data, word w)
{
   Integer i = getConstant(w).data.integer_data;
   Integer j = (Integer)data;
   return i->data == j->data;
}

int integer_compare2(void* data, int len, word w)
{
   Integer i = getConstant(w).data.integer_data;
   return i->data == *((long*)data);
}

int functor_compare1(void* data, word w)
{
   Functor g = (Functor)data;
   Functor f = getConstant(w).data.functor_data;
   return f->name == g->name && f->arity == g->arity;
}

int functor_compare2(void* data, int len, word w)
{
   Functor f = getConstant(w).data.functor_data;
   return f->name == *((word*)data) && f->arity == len;
}

void initialize_ctable()
{
   CTableSize = 256;
   map[ATOM_TYPE] = bihashmap_new(atom_compare1, atom_compare2);
   map[INTEGER_TYPE] = bihashmap_new(integer_compare1, integer_compare2);
   map[FUNCTOR_TYPE] = bihashmap_new(functor_compare1, functor_compare2);
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
      free(CTable);
      CTable = c;
   }
   w = (word)((CNext << 2) | CONSTANT_TAG);
   CTable[CNext].type = type;
   void* created = create(key1, key2);
   switch(type)
   {
      case ATOM_TYPE: CTable[CNext++].data.atom_data = (Atom)created; break;
      case FUNCTOR_TYPE: CTable[CNext++].data.functor_data = (Functor)created; break;
      case INTEGER_TYPE: CTable[CNext++].data.integer_data = (Integer)created; break;
   }

   if (isNew != NULL)
      *isNew = 1;
   bihashmap_put(map[type], hashcode, created, w);
   return w;
}
