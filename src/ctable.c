#include "global.h"
#include "types.h"
#include "kernel.h"
#include "ctable.h"
#include "constants.h"
#include "options.h"
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <gmp.h>

#define CONSTANT_BITS 2

constant* CTable = NULL;
bimap_t map[7];
int CTableSize = 0;
int CNext = 0;

void ctable_check()
{
   bihashmap_check(map[ATOM_TYPE]);
   /*
   printf("%d %d %d %d %d %d\n",
          bihashmap_length(map[ATOM_TYPE]),
          bihashmap_length(map[INTEGER_TYPE]),
          bihashmap_length(map[FUNCTOR_TYPE]),
          bihashmap_length(map[FLOAT_TYPE]),
          bihashmap_length(map[BIGINTEGER_TYPE]),
          bihashmap_length(map[RATIONAL_TYPE]));
   */
}


cdata getConstant(word w, int* type)
{
   constant c = CTable[w >> CONSTANT_BITS];
   if (type != NULL)
      *type = c.type;
   return c.data;
}

int getConstantType(word w)
{
   return CTable[w >> CONSTANT_BITS].type;
}


int atom_compare1(void* data, word w)
{
   Atom a = getConstant(w, NULL).atom_data;
   Atom b = (Atom)data;
   return(a->length == b->length && strncmp(b->data, a->data, a->length) == 0);
}

int atom_compare2(void* data, int len, word w)
{
   Atom a = getConstant(w, NULL).atom_data;
   return(a->length == len && strncmp(data, a->data, len) == 0);
}

int integer_compare1(void* data, word w)
{
   long i = getConstant(w, NULL).integer_data;
   long j = (long)data;
   return i == j;
}

int integer_compare2(void* data, int len, word w)
{
   long i = getConstant(w, NULL).integer_data;
   return i == *((long*)data);
}

int functor_compare1(void* data, word w)
{
   Functor g = (Functor)data;
   Functor f = getConstant(w, NULL).functor_data;
   return f->name == g->name && f->arity == g->arity;
}

int functor_compare2(void* data, int len, word w)
{
   Functor f = getConstant(w, NULL).functor_data;
   return f->name == *((word*)data) && f->arity == len;
}

int float_compare1(void* data, word w)
{
   Float g = (Float)data;
   Float f = getConstant(w, NULL).float_data;
   return f->data == g->data;
}

int float_compare2(void* data, int len, word w)
{
   Float f = getConstant(w, NULL).float_data;
   return f->data == *((double*)data);
}

int biginteger_compare1(void* data, word w)
{
   BigInteger g = (BigInteger)data;
   BigInteger f = getConstant(w, NULL).biginteger_data;
   return mpz_cmp(f->data, g->data) == 0;
}

int biginteger_compare2(void* data, int len, word w)
{
   BigInteger f = getConstant(w, NULL).biginteger_data;
   return mpz_cmp(f->data, *((mpz_t*)data)) == 0;
}

int rational_compare1(void* data, word w)
{
   Rational g = (Rational)data;
   Rational f = getConstant(w, NULL).rational_data;
   return mpq_cmp(f->data, g->data) == 0;
}

int rational_compare2(void* data, int len, word w)
{
   Rational f = getConstant(w, NULL).rational_data;
   return mpq_cmp(f->data, *((mpq_t*)data)) == 0;
}

void initialize_ctable()
{
   CTableSize = 256;
   map[ATOM_TYPE] = bihashmap_new(atom_compare1, atom_compare2);
   map[INTEGER_TYPE] = bihashmap_new(integer_compare1, integer_compare2);
   map[FUNCTOR_TYPE] = bihashmap_new(functor_compare1, functor_compare2);
   map[FLOAT_TYPE] = bihashmap_new(float_compare1, float_compare2);
   map[BIGINTEGER_TYPE] = bihashmap_new(biginteger_compare1, biginteger_compare2);
   map[RATIONAL_TYPE] = bihashmap_new(rational_compare1, rational_compare2);
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
   w = (word)((CNext << CONSTANT_BITS) | CONSTANT_TAG);
   CTable[CNext].type = type;
   void* created = create(key1, key2);
   switch(type)
   {
      case ATOM_TYPE: CTable[CNext++].data.atom_data = (Atom)created; break;
      case FUNCTOR_TYPE: CTable[CNext++].data.functor_data = (Functor)created; break;
      case INTEGER_TYPE: CTable[CNext++].data.integer_data = (long)created; break;
      case FLOAT_TYPE: CTable[CNext++].data.float_data = (Float)created; break;
      case BIGINTEGER_TYPE: CTable[CNext++].data.biginteger_data = (BigInteger)created; break;
      case RATIONAL_TYPE: CTable[CNext++].data.rational_data = (Rational)created; break;
      default:
         assert(0);
   }
   if (isNew != NULL)
      *isNew = 1;
//   if (type == INTEGER_TYPE)
//      printf("Interning %ld as %08lx (%ld)\n", *((long*)key1), w, (long)created);
   bihashmap_put(map[type], hashcode, created, w);
   return w;
}

// Blobs are a bit special since they are assumed to never be equal to each other
// This means we can skip the hashmap entirely - if you ever MAKE_BLOB then it is assumed you know that this is a new one
Blob allocBlob(char* type, void* ptr, char* (*portray)(char*, void*, Options*, int, int*))
{
   Blob b = malloc(sizeof(blob));
   b->type = strdup(type);
   b->ptr = ptr;
   b->portray = portray;
   return b;
}


word intern_blob(char* type, void* ptr, char* (*portray)(char*, void*, Options*, int, int*))
{
   word w = (word)((CNext << CONSTANT_BITS) | CONSTANT_TAG);
   Blob b = allocBlob(type, ptr, portray);
   if (CTableSize == CNext)
   {
      CTableSize <<= 1;
      constant* c = calloc(CTableSize, sizeof(constant));
      memcpy(c, CTable, CNext * sizeof(constant));
      free(CTable);
      CTable = c;
   }
   CTable[CNext].type = BLOB_TYPE;
   CTable[CNext++].data.blob_data = b;
   return w;
}
