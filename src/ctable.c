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
int constant_count = 0;

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

int next_free_index = -1;

int allocate_ctable_index(int type)
{
   if (next_free_index != -1)
   {
      int index = next_free_index;
      next_free_index = CTable[index].data.tombstone_data;
      //printf("Reusing index %d for a %d. Next free is %d\n", index, type, next_free_index);
      if (CNext < index+1)
         CNext = index+1;
      return index;
   }
   else if (CTableSize == CNext)
   {
      CTableSize <<= 1;
      constant* c = calloc(CTableSize, sizeof(constant));
      memcpy(c, CTable, CNext * sizeof(constant));
      free(CTable);
      CTable = c;
   }
   int index = CNext;
   CNext++;
   return index;
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
   int index = allocate_ctable_index(type);
   w = (word)((index << CONSTANT_BITS) | CONSTANT_TAG);
   CTable[index].type = type;
   CTable[index].references = 1;
   void* created = create(key1, key2);
   switch(type)
   {
      case ATOM_TYPE: CTable[index].data.atom_data = (Atom)created; break;
      case FUNCTOR_TYPE: CTable[index].data.functor_data = (Functor)created; break;
      case INTEGER_TYPE: CTable[index].data.integer_data = (long)created; break;
      case FLOAT_TYPE: CTable[index].data.float_data = (Float)created; break;
      case BIGINTEGER_TYPE: CTable[index].data.biginteger_data = (BigInteger)created; break;
      case RATIONAL_TYPE: CTable[index].data.rational_data = (Rational)created; break;
      default:
         assert(0);
   }
   if (isNew != NULL)
      *isNew = 1;
   constant_count++;
   bihashmap_put(map[type], hashcode, created, w);
   return w;
}

// Blobs are a bit special since they are assumed to never be equal to each other
// This means we can skip the hashmap entirely - if you ever MAKE_BLOB then it is assumed you know that this is a new one
Blob allocBlob(const char* type, void* ptr, char* (*portray)(char*, void*, Options*, int, int*))
{
   Blob b = malloc(sizeof(blob));
   b->type = strdup(type);
   b->ptr = ptr;
   b->portray = portray;
   return b;
}

void freeBlob(Blob b)
{
   free(b->type);
   free(b);
}


word intern_blob(const char* type, void* ptr, char* (*portray)(char*, void*, Options*, int, int*))
{
   int index = allocate_ctable_index(BLOB_TYPE);
   word w = (word)((index << CONSTANT_BITS) | CONSTANT_TAG);
   Blob b = allocBlob(type, ptr, portray);
   CTable[index].type = BLOB_TYPE;
   CTable[index].references = 2;
   CTable[index].data.blob_data = b;
   constant_count++;
   return w;
}

void delete_constant(int index)
{
   constant c = CTable[index];
   switch(c.type)
   {
      case ATOM_TYPE:
      {
         Atom a = c.data.atom_data;
         bihashmap_remove(map[ATOM_TYPE], uint32_hash((unsigned char*)a->data, a->length), a->data, a->length);
         free(a->data);
         free(a);
         break;
      }
      case FUNCTOR_TYPE:
      {
         Functor f = c.data.functor_data;
         Atom name = getConstant(f->name, NULL).atom_data;
         bihashmap_remove(map[FUNCTOR_TYPE], uint32_hash((unsigned char*)name->data, name->length) + f->arity, &f->name, f->arity);
         release_constant(f->name);
         free(f);
      }
      case INTEGER_TYPE:
      {
         long i = c.data.integer_data;
         bihashmap_remove(map[INTEGER_TYPE], long_hash(i), &i, sizeof(long));
         break;
      }
      case FLOAT_TYPE:
      {
         Float f = c.data.float_data;
         bihashmap_remove(map[FLOAT_TYPE], hash64(*((uint64_t*)&f->data)), &f->data, sizeof(double));
         free(f);
      }
      case BIGINTEGER_TYPE:
      {
         BigInteger bi = c.data.biginteger_data;
         bihashmap_remove(map[BIGINTEGER_TYPE], hashmpz(bi), bi, 77);
         free(bi);
      }
      case RATIONAL_TYPE:
      {
         Rational r = c.data.rational_data;
         bihashmap_remove(map[RATIONAL_TYPE], hashmpq(r), r, 0);
         free(r);
      }
      case BLOB_TYPE:
         // We do not have a bihashmap for blobs since they never unify
         freeBlob(c.data.blob_data);
         break;
      default:
         assert(0);
   }
   // Now delete the reference in the table
   CTable[index].type = TOMBSTONE_TYPE;
   CTable[index].data.tombstone_data = next_free_index;
   next_free_index = index;
   constant_count--;

}

word acquire_constant(word w)
{
   assert(TAGOF(w) == CONSTANT_TAG);
   CTable[w >> CONSTANT_BITS].references++;
   printf("Acquiring constant "); PORTRAY(w); printf(" which now has %d references\n", CTable[w >> CONSTANT_BITS].references);
   // This return value makes it easier to chain things together. You can do something like
   //   return acquire_constant(MAKE_ATOM("foo"))
   return w;
}

void release_constant(word w)
{
   assert(TAGOF(w) == CONSTANT_TAG);
   CTable[w >> CONSTANT_BITS].references--;
   printf("Releasing constant "); PORTRAY(w); printf(" which now has %d references\n", CTable[w >> CONSTANT_BITS].references);
   // Step E of AGC
   if (CTable[w >> CONSTANT_BITS].references == 0)
   {
      printf("Deleting constant "); PORTRAY(w); printf("\n");
      delete_constant(w >> CONSTANT_BITS);
   }
}

int get_constant_count()
{
   return constant_count;
}

void garbage_collect_constants()
{
   /*
     The AGC algorithm here is quite simple, but it makes use of things like the fact we know that the engine is single-threaded
     1) whenever a constant is created, it gets a reference count of 1.
        1A) If a functor is created, then the atom representing its name has its reference count incremented by 1.
     2) whenever a constant is used in a compiled clause, it bumps the reference count by 1
     3) Whenever a constant is copied into a local term, it bumps the reference count by 1
     4) Whenever a clause is freed, any constants in its constant table has its reference count reduced by 1
     5) Whenever a local term is freed, constants in the local term has its reference count reduced by 1
     6) Whenever a functor is freed, the atom representing its name has its reference count reduced by 1
     7) garbage_collect_constants() can be called at any time to collect unused constants. It proceeds as follows:
        A) Seeep through all constants in ctable. For all non-tombstone references, reduce their reference count by 1.
        B) Sweep through ARGS, incrementing the references of all constants found by 1. Do not follow pointers.
        C) Sweep from HEAP to H, incrementing the references of all constants found by 1. Do not follow pointers.
        D) Sweep through all constants in ctable. For all non-tombstone references with a reference count of 0, delete the constant.
        E) *Except* in the initial sweep in step A, whenever we reduce the reference count of a constant, if it becomes zero, we can free it.

    Because functors increment the reference count of their atom, we will not try and free an atom that is used by a functor until after the functor itself is freed.
    It might be simpler to just wait until the next pass to clean these atoms up, but I think this approach is still safe.
   */

   // Step A
   for (int i = 0; i < CNext; i++)
   {
      if (CTable[i].type != TOMBSTONE_TYPE)
         CTable[i].references--;
   }

   // Step B (FIXME: currently skipped. I dont know how to determine which ARGS slots are in use)

   // Step C
   for (word* i = HEAP; i < H; i++)
   {
      if (TAGOF(*i) == CONSTANT_TAG)
         acquire_constant(*i);
   }

   // Step D
   for (int i = 0; i < CNext; i++)
   {
      if (CTable[i].type != TOMBSTONE_TYPE && CTable[i].references == 0)
      {
         printf("Constant %d is garbage: \n"); PORTRAY((word)((i << CONSTANT_BITS) | CONSTANT_TAG));
         delete_constant(i);
      }
   }


}
