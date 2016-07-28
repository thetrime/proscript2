#include <stdint.h>
#include <stdlib.h>

#include "types.h"


// NFR is a pointer to the next frame
// FR is a pointer to the current frame
// ARGP is now a pointer. There is no longer an ARGI

#define CODE16(t) ((*(t) << 8) | (*(t+1)))
#define CODE32(t) ((*(t) << 24) | ((*(t+1)) << 16) | ((*(t+2)) << 8) | (*(t+3)))
#define FUNCTOR_VALUE(t) ((Functor)CTable[t])
#define FUNCTOROF(t) (*((Word)t))
#define ARGOF(t, i) (*((Word)(t+i+1)))
#define ARGPOF(t, i) ((Word)(t+i+1))

#define VARIABLE_TAG 0x00000000
#define COMPOUND_TAG 0x80000000
#define CONSTANT_TAG 0xc0000000
#define TAG_MASK     0xc0000000
#define TAGOF(t) (t & TAG_MASK)

word MAKE_VAR();
word MAKE_ATOM(char* data);
word MAKE_INTEGER(long data);
word MAKE_FUNCTOR(word name, int arity);
word MAKE_VCOMPOUND(word functor, ...);
void PORTRAY(word w);

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
