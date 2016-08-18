#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

size_t allocated = 0;
int qqqz = 0;

void* trace_malloc(size_t size, char* location)
{
   //assert(size < 1000000);
   allocated += size;
   void* ptr = malloc(size + sizeof(size_t));
   size_t* p = (size_t*)ptr;
   void* userptr = (void*)&p[1];
//#ifdef MEMTRACE_VERBOSE
   if (qqqz)
      printf("%s: Allocated %zd bytes at %p (actual pointer for free is %p) (total: %zd)\n", location, size, userptr, p, allocated);
//#endif
   p[0] = size;
   return userptr;
}

void* trace_calloc(size_t size, size_t elemsize, char* where)
{
   void* ptr = trace_malloc(size*elemsize, where);
   memset(ptr, 0, size*elemsize);
   return ptr;
}

void trace_free(void* ptr, char* where)
{
   size_t* p = (size_t*)ptr;
   size_t s = p[-1];
   allocated -= s;
//#ifdef MEMTRACE_VERBOSE
   if (qqqz)
      printf("%s: Freeing %zd bytes from %p (total: %zd)\n", where, s, ptr, allocated);
//#endif
   free(&p[-1]);
}

void trace_free_gmp(void* ptr, size_t size)
{
   trace_free(ptr, "GMP");
}

void* trace_malloc_gmp(size_t size)
{
   return trace_malloc(size, "GMP");
}


void* trace_realloc_gmp(void* userptr, size_t oldsize, size_t newsize)
{
   if (userptr == NULL)
      return trace_malloc_gmp(newsize);
   void* newptr = trace_malloc(newsize, "GMP");
   memcpy(newptr, userptr, oldsize);
   trace_free(userptr, "GMP");
   return newptr;
}

void* trace_realloc(void* userptr, size_t newsize, char* where)
{
   if (userptr == NULL)
      return trace_malloc(newsize, where);
   size_t* p = (size_t*)userptr;
   size_t oldsize = p[-1];
   void* newptr = trace_malloc(newsize, where);
   memcpy(newptr, userptr, oldsize);
   trace_free(userptr, where);
   return newptr;
}

char* trace_strdup(const char* source, char* where)
{
   char* ptr = trace_malloc(strlen(source)+1, where);
   memcpy(ptr, source, strlen(source)+1);
   return ptr;
}

char* trace_strndup(const char* source, size_t len, char* where)
{
   char* ptr = trace_malloc(len+1, where);
   memcpy(ptr, source, len+1);
   return ptr;
}

void print_memory_info()
{
   printf("Total allocated bytes: %zd\n", allocated);
   qqqz = 1;
}
