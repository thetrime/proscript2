#ifndef _KERNEL_H
#define _KERNEL_H
#include <stdint.h>
#include <stdlib.h>
#include "list.h"
#include "types.h"


// NFR is a pointer to the next frame
// FR is a pointer to the current frame
// ARGP is now a pointer. There is no longer an ARGI

#define CODE16(t) ((*(t) << 8) | (*(t+1)))
#define CODE32(t) ((*(t) << 24) | ((*(t+1)) << 16) | ((*(t+2)) << 8) | (*(t+3)))
#define FUNCTOR_VALUE(t) ((Functor)CTable[t])
#define FUNCTOROF(t) (*((Word)(t & ~TAG_MASK)))
#define ARGOF(t, i) DEREF(((word)(((Word*)(t & ~TAG_MASK))+i+1)))
#define ARGPOF(t) ((Word)(((Word*)(t & ~TAG_MASK))+1)) // FIXME: These cannot both be right!

#define VARIABLE_TAG 0b00
#define COMPOUND_TAG 0b10
#define CONSTANT_TAG 0b11
#define TAG_MASK     0b11
#define TAGOF(t) (t & TAG_MASK)

word DEREF(word t);
word MAKE_VAR();
word MAKE_ATOM(char* data);
word MAKE_NATOM(char* data, size_t length);
word MAKE_INTEGER(long data);
word MAKE_FUNCTOR(word name, int arity);
word MAKE_VCOMPOUND(word functor, ...);
word MAKE_LCOMPOUND(word functor, List* args);
void PORTRAY(word w);
void SET_EXCEPTION(word);

enum OPCODE
{
#define INSTRUCTION(a) a,
#define INSTRUCTION_CONST(a) a,
#define INSTRUCTION_SLOT(a) a,
#define INSTRUCTION_ADDRESS(a) a,
#define INSTRUCTION_SLOT_ADDRESS(a) a,
#define END_INSTRUCTIONS NOP
#include "instructions"
#undef INSTRUCTION
#undef INSTRUCTION_CONST
#undef INSTRUCTION_SLOT
#undef INSTRUCTION_ADDRESS
#undef INSTRUCTION_SLOT_ADDRESS
#undef END_INSTRUCTIONS
};


typedef struct
{
   const char* name;
   int flags;
} instruction_info_t;

#define HAS_CONST 1
#define HAS_SLOT 2
#define HAS_ADDRESS 4

extern instruction_info_t instruction_info[];

enum MODE
{
   READ,
   WRITE,
} mode;

typedef enum
{
   SUCCESS,
   SUCCESS_WITH_CHOICES,
   FAIL,
   YIELD,
   ERROR,
   HALT
} RC;

RC execute_query(word);
RC backtrack_query();

#endif
