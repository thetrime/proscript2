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
#define ARGOF(t, i) ((word)(((Word*)(t & ~TAG_MASK))+i+1))
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
   I_FAIL,
   I_ENTER,
   I_EXIT_QUERY,
   I_EXITCATCH,
   I_FOREIGN,
   I_FOREIGNRETRY,
   I_EXIT,
   I_EXITFACT,
   I_DEPART,
   B_CLEANUP_CHOICEPOINT,
   B_THROW,
   B_THROW_FOREIGN,
   I_SWITCH_MODULE,
   I_EXITMODULE,
   I_CALL,
   I_CATCH,
   I_USERCALL,
   I_CUT,
   C_CUT,
   C_LCUT,
   C_IFTHEN,
   C_IFTHENELSE,
   I_UNIFY,
   B_FIRSTVAR,
   B_ARGVAR,
   B_VAR,
   B_POP,
   B_ATOM,
   B_VOID,
   B_FUNCTOR,
   H_FIRSTVAR,
   H_FUNCTOR,
   H_POP,
   H_ATOM,
   H_VOID,
   H_VAR,
   C_JUMP,
   C_OR,
   TRY_ME_OR_NEXT_CLAUSE,
   TRUST_ME,
   S_QUALIFY
};

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
#endif
