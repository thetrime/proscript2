#define COLLECT_GARBAGE 1

#ifdef COLLECT_GARBAGE
#if UINTPTR_MAX == 0xffffffffffffffff
#define CODEPTR(t) CODE64(t)
#define M_MASK 0x8000000000000000
#define F_MASK 0x4000000000000000
#elif UINTPTR_MAX == 0xffffffff
#define CODEPTR(t) CODE32(t)
#define M_MASK 0x80000000
#define F_MASK 0x40000000
#elif UINTPTR_MAX == 0xffff
#define CODEPTR(t) CODE16(t)
#define M_MASK 0x8000
#define F_MASK 0x4000
#endif

void gc();

#endif
