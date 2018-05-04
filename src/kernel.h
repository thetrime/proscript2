#ifndef _KERNEL_H
#define _KERNEL_H
#include <stdint.h>
#include <stdlib.h>
#include "list.h"
#include "types.h"
#include "stream.h"
#include <gmp.h>
#include <stdarg.h>


// NFR is a pointer to the next frame
// FR is a pointer to the current frame
// ARGP is now a pointer. There is no longer an ARGI

#define CODE16(t) ((*(t) << 8) | (*(t+1)))
#define CODE32(t) ((*(t) << 24) | ((*(t+1)) << 16) | ((*(t+2)) << 8) | (*(t+3)))
#define CODE64(t) ((((word)*(t)) << 56)| (((word)*(t+1)) << 48)| (((word)*(t+2)) << 40)| (((word)*(t+3)) << 32)| (((word)*(t+4)) << 24)| (((word)*(t+5)) << 16)| (((word)*(t+6)) <<  8)| (((word)*(t+7)) <<  0))

#if UINTPTR_MAX == 0xffffffffffffffff
#define CODEPTR(t) CODE64(t)
#define PRIwx "lx"
#define PRIwd "ld"
#define PRIpd "lu"
#elif UINTPTR_MAX == 0xffffffff
#define CODEPTR(t) CODE32(t)
#define PRIwx "x"
#define PRIwd "d"
#define PRIpd "u"
#elif UINTPTR_MAX == 0xffff
#define CODEPTR(t) CODE16(t)
#define PRIwx "x"
#define PRIwd "d"
#define PRIpd "u"
#endif

#define FUNCTOR_VALUE(t) ((Functor)CTable[t])
#define FUNCTOROF(t) (*((Word)(t & ~TAG_MASK)))
#define ARGOF(t, i) DEREF(((word)(((Word*)(DEREF(t) & ~TAG_MASK))+i+1)))
#define ARGPOF(t) ((Word)(((Word*)(t & ~TAG_MASK))+1)) // FIXME: These cannot both be right!

#define VARIABLE_TAG 0b00
#define POINTER_TAG  0b01
#define COMPOUND_TAG 0b10
#define CONSTANT_TAG 0b11
#define TAG_MASK     0b11
#define TAGOF(t) (t & TAG_MASK)

word DEREF(word t);
word MAKE_VAR();
word MAKE_BIGINTEGER(mpz_t data);
word MAKE_RATIONAL(mpq_t data);
word MAKE_ATOM(char* data);
word MAKE_NATOM(char* data, size_t length);
word MAKE_BLOB(char* type, void* data);
word MAKE_INTEGER(long data);
word MAKE_FLOAT(double data);
word MAKE_FUNCTOR(word name, int arity);
word MAKE_VCOMPOUND(word functor, ...);
word MAKE_VACOMPOUND(word functor, va_list args);
word MAKE_LCOMPOUND(word functor, List* args);
word MAKE_ACOMPOUND(word functor, word* args);
void PORTRAY(word w);
int SET_EXCEPTION(word);
void CLEAR_EXCEPTION();

enum OPCODE
{
#define INSTRUCTION(a) a,
#define INSTRUCTION_CONST(a) a,
#define INSTRUCTION_CONST_SLOT(a) a,
#define INSTRUCTION_SLOT(a) a,
#define INSTRUCTION_ADDRESS(a) a,
#define INSTRUCTION_SLOT_ADDRESS(a) a,
#define END_INSTRUCTIONS NOP
#include "instructions"
#undef INSTRUCTION
#undef INSTRUCTION_CONST
#undef INSTRUCTION_CONST_SLOT
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


typedef enum
{
   FAIL = 0,
   SUCCESS = 1,
   SUCCESS_WITH_CHOICES = 2,
   YIELD = 3,
   ERROR = 4,
   HALT,
   AGAIN
} RC;

#define PREDICATE_FOREIGN 1
#define PREDICATE_DYNAMIC 2

typedef void(*ExecutionCallback)(RC);

void execute_query(word, ExecutionCallback);
void backtrack_query(ExecutionCallback);
word getException();

int clause_functor(word, word*);
void consult_string(char*);
void consult_string_of_length(char*, int len);
int consult_file(const char*);
void consult_stream(Stream);
void print_clause(Clause);
void initialize_kernel();
int unify(word a, word b);
int unify_or_undo(word a, word b);
int safe_unify(word a, word b); // Handles a or b being local copies
word copy_term(word term);
#define NON_DETERMINISTIC 1

void make_foreign_choicepoint(word);
void make_foreign_choicepoint_v(word, ...);

// NOTE: make_foreign_choicepoint() is NOT well-tested!
void make_foreign_cleanup_choicepoint(word, void (*fn)(int, word), int);

int head_functor(word clause, Module* module, word* functor);
word predicate_indicator(word term);
word MAKE_POINTER(void* data);
void* GET_POINTER(word data);
Module get_current_module();
void halt(int code);
ExecutionCallback current_yield();
void resume_yield(RC status, ExecutionCallback y);

extern Stream current_input;
extern Stream current_output;
extern word HEAP[];
#endif

word copy_local_with_extra_space(word t, word** local, int extra);
word copy_local(word t, word** local);
word get_choicepoint_depth();
Choicepoint push_state();
void restore_state(Choicepoint state);
void hard_reset();
void qqq();
long heap_usage();
