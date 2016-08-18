#include <stdlib.h>
#include <string.h>

#ifdef MEMTRACE
#define TRACE2(f,l) ( f ":" #l)
#define TRACE1(f,l) TRACE2(f,l)
#define TRACE() TRACE1(__FILE__, __LINE__)

#define malloc(p) trace_malloc(p, TRACE())
#define free(p) trace_free(p, TRACE())
#define calloc(p,t) trace_calloc(p,t, TRACE())
#define realloc(p,s) trace_realloc(p,s, TRACE())
#define strdup(p) trace_strdup(p, TRACE())
#define strndup(p,n) trace_strndup(p, n, TRACE())
#endif

#include <stddef.h>

void* trace_malloc_gmp(size_t size);
void* trace_malloc(size_t size, char*);
void* trace_calloc(size_t size, size_t elemsize, char*);
void* trace_realloc(void* ptr, size_t size, char*);
void* trace_realloc_gmp(void* ptr, size_t oldsize, size_t newsize);
void trace_free(void* ptr, char*);
void trace_free_gmp(void* ptr, size_t size);
char* trace_strdup(const char* ptr, char*);
char* trace_strndup(const char* ptr, size_t len, char*);

void print_memory_info();
