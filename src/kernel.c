#include "global.h"
#include "builtin.h"
/* Some important points

   Generally speaking, a choicepoint accessible to a frame appears higher in the stack than the frame
   However, there is one important exception: the 'Head' choicepoint created by a TRY_ME_OR_NEXT_CLAUSE instruction
   Because we already created the frame, it is too late to put the choicepoint before it (unless we move the frame up)
   In the interest of efficiency, this choicepoint is created AFTER the frame it relates to

 */

#define RECORD_HEAP_USAGE  (void)0
//#define RECORD_HEAP_USAGE  do {if (H > HMAX) {HMAX = H;}} while(0)


#ifdef EMSCRIPTEN
#include <emscripten/emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include "kernel.h"
#include "constants.h"
#include "ctable.h"
#include "crc.h"
#include "compiler.h"
#include "errors.h"
#include "module.h"
#include "stream.h"
#include "parser.h"
#include "prolog_flag.h"
#include "foreign.h"
#include "checks.h"

#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

enum MODE
{
   READ,
   WRITE,
} mode;


instruction_info_t instruction_info[] = {
#define INSTRUCTION(a) {#a, 0},
#define INSTRUCTION_CONST(a) {#a, HAS_CONST},
#define INSTRUCTION_CONST_SLOT(a) {#a, HAS_CONST | HAS_SLOT},
#define INSTRUCTION_SLOT(a) {#a, HAS_SLOT},
#define INSTRUCTION_ADDRESS(a) {#a, HAS_ADDRESS},
#define INSTRUCTION_SLOT_ADDRESS(a) {#a, HAS_ADDRESS | HAS_SLOT},
#define END_INSTRUCTIONS {"NOP", 0}
#include "instructions"
#undef INSTRUCTION
#undef INSTRUCTION_CONST
#undef INSTRUCTION_CONST_SLOT
#undef INSTRUCTION_SLOT
#undef INSTRUCTION_ADDRESS
#undef INSTRUCTION_SLOT_ADDRESS
#undef END_INSTRUCTIONS
};

#define AFTER_FRAME(p) &p->slots[p->clause->slot_count];
#define AFTER_CHOICE(p) &p->args[p->argc];


int debugging = 0;


uint8_t* PC;
int halted = 0;
Choicepoint CP = NULL;
#define HEAP_SIZE 655350
#define TRAIL_SIZE 327675
#define STACK_SIZE 65535
#define ARG_STACK_SIZE 512

word TRAIL[TRAIL_SIZE];
word* TTOP = &TRAIL[TRAIL_SIZE];
word* TR = &TRAIL[0];
word HEAP[HEAP_SIZE];
word* HTOP = &HEAP[HEAP_SIZE];
word* H = &HEAP[0];
word* HMAX = &HEAP[0];
word STACK[STACK_SIZE];
word* STOP = &STACK[STACK_SIZE];
word* SP = &STACK[0];
uintptr_t argStack[ARG_STACK_SIZE];
uintptr_t* argStackP = &argStack[0];
uintptr_t* argStackTop = &argStack[ARG_STACK_SIZE];
word ARGS[512];
word* ATOP = &ARGS[512];

ExecutionCallback current_yield_ptr = NULL;
Frame FR, NFR;
word *ARGP;
Module currentModule = NULL;
Module userModule = NULL;
Choicepoint initialChoicepoint = NULL;
word current_exception = 0;
word* exception_local = NULL;
Stream current_input = NULL;
Stream current_output = NULL;

void print_choices()
{
   Choicepoint q = CP;
   printf("Choices: ----\n");
   while(q != NULL)
   {
      printf("    @ %p: Retry at %p\n", q, q->PC);
      q = q->CP;
   }
   printf("-----End choices\n");

}

void print_instruction();
void fatal(char* string)
{
   printf("Fatal: %s\n", string);
   assert(0);
}

EMSCRIPTEN_KEEPALIVE
word MAKE_VAR()
{
   word newVar = (word)H;
   *H = newVar;
   assert(H < HTOP);
   H++;
   RECORD_HEAP_USAGE;
   return newVar;
}

word MAKE_COMPOUND(word functor)
{
   word addr = (word)H;
   *H = functor;
   H++;
   // Make all the args vars
   int arity = getConstant(functor, NULL).functor_data->arity;
   assert(H + arity < HTOP);
   for (int i = 0; i < arity; i++)
   {
      MAKE_VAR();
   }
   RECORD_HEAP_USAGE;
   return addr | COMPOUND_TAG;
}

word MAKE_VACOMPOUND(word functor, va_list argp)
{
   Functor f = getConstant(functor, NULL).functor_data;
   word addr = (word)H;
   *H = functor;
   H++;
   assert(H + f->arity < HTOP);
   for (int i = 0; i < f->arity; i++)
   {
      word w = va_arg(argp, word);
      *(H++) = w;
   }
   RECORD_HEAP_USAGE;
   va_end(argp);
   return addr | COMPOUND_TAG;
}

word MAKE_VCOMPOUND(word functor, ...)
{
   va_list argp;
   va_start(argp, functor);
   return MAKE_VACOMPOUND(functor, argp);
}


// You MUST call MAKE_ACOMPOUND with a functor as the first arg
EMSCRIPTEN_KEEPALIVE
word MAKE_ACOMPOUND(word functor, word* values)
{
   int type;
   cdata c = getConstant(functor, &type);
   assert(type == FUNCTOR_TYPE);
   Functor f = c.functor_data;
   word addr = (word)H;
   *H = functor;
   H++;
   assert(H + f->arity < HTOP);
   for (int i = 0; i < f->arity; i++)
      *(H++) = values[i];
   assert(H < HTOP);
   RECORD_HEAP_USAGE;
   return addr | COMPOUND_TAG;
}

void _lcompoundarg(word w, void* ignored)
{
   *(H++) = w;
}

// You can call MAKE_LCOMPOUND with either an atom or a functor as the first arg.
// If you call it with an atom, it will make a functor with arity list->size for you
word MAKE_LCOMPOUND(word functor, List* list)
{
   int type;
   cdata c = getConstant(functor, &type);
   if (type == ATOM_TYPE)
      functor = MAKE_FUNCTOR(functor, list_length(list));
   Functor f = getConstant(functor, NULL).functor_data;
   word addr = (word)H;
   *(H++) = functor;
   assert(H + list_length(list) < HTOP);
   list_apply(list, NULL, _lcompoundarg);
   RECORD_HEAP_USAGE;
   return addr | COMPOUND_TAG;
}

void _make_backtrace(word cell, void* result)
{
   word* r = (word*)result;
   *r = MAKE_VCOMPOUND(listFunctor, predicate_indicator(DEREF(cell)), *r);
}

word make_backtrace(List* list)
{
   word result = emptyListAtom;
   list_apply(list, &result, _make_backtrace);
   return result;
}

void* allocAtom(void* data, int length)
{
   Atom a = malloc(sizeof(atom));
   a->data = strndup(data, length);
   a->length = length;
   return a;
}


void* allocInteger(void* data, int len)
{
   return (void*)*((long*)data);
   //Integer a = malloc(sizeof(integer));
   //a->data = *((long*)data);
   //return a;
}

void* allocBigInteger(void* data, int len)
{
   BigInteger a = malloc(sizeof(biginteger));
   mpz_init(a->data);
   mpz_set(a->data, *(mpz_t*)data);
   return a;
}

void* allocRational(void* data, int len)
{
   Rational a = malloc(sizeof(rational));
   mpq_init(a->data);
   mpq_set(a->data, *((mpq_t*)data));
   return a;
}

void* allocFloat(void* data, int len)
{
   Float a = malloc(sizeof(_float));
   a->data = *((double*)data);
   return a;
}

void* allocFunctor(void* data, int len)
{
   Functor f = malloc(sizeof(functor));
   f->name = *((word*)data);
   f->arity = len;
   return f;
}

uint32_t hash64(uint64_t key)
{
   key = (~key) + (key << 18);
   key = key ^ (key >> 31);
   key = key * 21;
   key = key ^ (key >> 11);
   key = key + (key << 6);
   key = key ^ (key >> 22);
   return (uint32_t) key;
}

uint32_t hash32(uint32_t key)
{
   key = ((key >> 16) ^ key) * 0x45d9f3b;
   key = ((key >> 16) ^ key) * 0x45d9f3b;
   key = (key >> 16) ^ key;
   return key;
}

#ifdef mpz_limbs_read
uint32_t hashmpz(mpz_t key)
{
   mp_limb_t hash = 0;
   const mp_limb_t * limbs =  mpz_limbs_read(key);
   for (int i = 0; i < mpz_size(key); i++)
      hash ^= limbs[i];
   if (sizeof(mp_limb_t) == 8)
      return hash64(hash);
   else if (sizeof(mp_limb_t) == 4)
      return hash32(hash);
   else
      assert(0 && "Cannot deal with mp_limb size");
}
#else
uint32_t hashmpz(mpz_t key)
{
   // Oh well, we tried
   char* str = mpz_get_str(NULL, 10, key);
   uint32_t result = uint32_hash((unsigned char*)str, strlen(str));
   free(str);
   return result;
}
#endif

uint32_t hashmpq(mpq_t key)
{
   uint32_t h = 0;
   mpz_t n;
   mpz_init(n);
   mpq_get_num(n, key);
   h = hashmpz(n);
   mpz_clear(n);
   mpz_init(n);
   mpq_get_den(n, key);
   h ^= hashmpz(n);
   mpz_clear(n);
   return h;
}

EMSCRIPTEN_KEEPALIVE
word MAKE_NATOM(char* data, size_t length)
{
   return intern(ATOM_TYPE, uint32_hash((unsigned char*)data, length), data, length, allocAtom, NULL);
}

EMSCRIPTEN_KEEPALIVE
word MAKE_BIGINTEGER(mpz_t i)
{
   return intern(BIGINTEGER_TYPE, hashmpz(i), i, 77, allocBigInteger, NULL);
}

EMSCRIPTEN_KEEPALIVE
word MAKE_RATIONAL(mpq_t i)
{
   return intern(RATIONAL_TYPE, hashmpq(i), i, 0, allocRational, NULL);
}

EMSCRIPTEN_KEEPALIVE
word MAKE_ATOM(char* data)
{
   return MAKE_NATOM(data, strlen(data));
}

EMSCRIPTEN_KEEPALIVE
word MAKE_BLOB(char* type, void* ptr)
{
   return intern_blob(type, ptr, NULL);
}


EMSCRIPTEN_KEEPALIVE
word MAKE_INTEGER(long data)
{
   return intern(INTEGER_TYPE, long_hash(data), &data, sizeof(long), allocInteger, NULL);
}

EMSCRIPTEN_KEEPALIVE
word MAKE_FLOAT(double data)
{
   return intern(FLOAT_TYPE, hash64(*((uint64_t*)&data)), &data, sizeof(double), allocFloat, NULL);
}


EMSCRIPTEN_KEEPALIVE
word MAKE_POINTER(void* data)
{
   return (word)data | POINTER_TAG;
}

EMSCRIPTEN_KEEPALIVE
void* GET_POINTER(word data)
{
   return (void*)(data & ~TAG_MASK);
}


EMSCRIPTEN_KEEPALIVE
word MAKE_FUNCTOR(word name, int arity)
{
   Atom n = getConstant(name, NULL).atom_data;
   word w =  intern(FUNCTOR_TYPE, uint32_hash((unsigned char*)n->data, n->length) + arity, &name, arity, allocFunctor, NULL);
   return w;
}

void _bind(word var, word value)
{
   //printf("TR: %d\n", TR);
   assert(TR < TTOP);
   *(TR++) = var;
   *((Word)var) = value;
}

EMSCRIPTEN_KEEPALIVE
word DEREF(word t)
{
   word t1;
   while (TAGOF(t) == VARIABLE_TAG)
   {
      t1 = *((Word)t);
      if (t1 == t)
         return t;
      t = t1;
   }
   return t;
}

void print_chain(word t)
{
   printf(" %08" PRIwx, t);
   word t1;
   while (TAGOF(t) == VARIABLE_TAG)
   {
      t1 = *((Word)t);
      printf(" -> %08" PRIwx, t1);
      if (t1 == t)
      {
         printf("(self)\n");
         return;
      }
      t = t1;
   }
   PORTRAY(t); printf("\n");
}

void PORTRAY(word w)
{
   w = DEREF(w);
   if (TAGOF(w) == VARIABLE_TAG)
   {
      if (w > (word)STACK)
         printf("_L%" PRIuPTR, (w - (word)STACK));
      else
         printf("_G%" PRIuPTR, (w - (word)HEAP));
   }
   else if (TAGOF(w) == CONSTANT_TAG)
   {
      int type;
      cdata c = getConstant(w, &type);
      switch(type)
      {
         case ATOM_TYPE:
            printf("%.*s", c.atom_data->length, c.atom_data->data);
            break;
         case INTEGER_TYPE:
            printf("%ld", c.integer_data);
            break;
         case FUNCTOR_TYPE:
            PORTRAY(c.functor_data->name); printf("/%d", c.functor_data->arity);
            break;
         case FLOAT_TYPE:
            printf("%f", c.float_data->data);
            break;
         case BLOB_TYPE:
            printf("<%s>(%p)", c.blob_data->type, c.blob_data->ptr);
            break;
         case BIGINTEGER_TYPE:
         {
            char* str = mpz_get_str(NULL, 10, c.biginteger_data->data);
            printf("%s", str);
            free(str);
            break;
         }
      default:
            printf("Unknown type: %d\n", type);
            assert(0);

      }
   }
   else if (TAGOF(w) == COMPOUND_TAG)
   {
      Functor functor = getConstant(FUNCTOROF(w), NULL).functor_data;
      PORTRAY(functor->name);
      printf("(");
      for (int i = 0; i < functor->arity; i++)
      {
         PORTRAY(ARGOF(w, i));
         if (i+1 < functor->arity)
            printf(",");
      }
      printf(")");
   }
   else if (TAGOF(w) == POINTER_TAG)
   {
      printf("#%p", GET_POINTER(w));
   }
   else
      printf("Bad tag\n");
}



int SET_EXCEPTION(word w)
{
   current_exception = copy_local(w, &exception_local);
   //printf("Exception has been set to "); PORTRAY(w); printf("\n");
   return 0;
}

void CLEAR_EXCEPTION()
{
   if (exception_local != NULL)
      free(exception_local);
   exception_local = NULL;
   current_exception = 0;
}

EMSCRIPTEN_KEEPALIVE
// safe_unify is used by the FLI.
// It is possible for one of a or b to be a variable on the heap, and the other to be a local-storage term
// This is dangerous (to say the least)
// When executing Prolog code, we know that this will never happen, since the compiler would have emitted UNSAFE_* terms if the
// unifier was on the stack (and we can never have anything outside of the heap/stack like we can with the FLI)
// So ordinary prolog code can just run unify() but FLI code must run safe_unify()
int safe_unify(word a, word b)
{
   a = DEREF(a);
   b = DEREF(b);
   if ((TAGOF(a) == COMPOUND_TAG) && (((Word)a) > HTOP || (Word)a < HEAP))
      a = copy_term(a);
   if ((TAGOF(b) == COMPOUND_TAG) && (((Word)b) > HTOP || (Word)b < HEAP))
      b = copy_term(b);
   return unify(a, b);
}


int unify(word a, word b)
{
   a = DEREF(a);
   b = DEREF(b);
   if (a == b)
      return SUCCESS;
   if (TAGOF(a) == VARIABLE_TAG)
   {
      if (TAGOF(b) == VARIABLE_TAG && b > a)
         _bind(b, a);
      else
         _bind(a, b);
      return SUCCESS;
   }
   if (TAGOF(b) == VARIABLE_TAG)
   {
      _bind(b, a);
      return SUCCESS;
   }
   if ((TAGOF(a) == COMPOUND_TAG) && (TAGOF(b) == COMPOUND_TAG) && (FUNCTOROF(a) == FUNCTOROF(b)))
   {
      int arity = getConstant(FUNCTOROF(a), NULL).functor_data->arity;
      for (int i = 0; i < arity; i++)
         if (!unify(ARGOF(a, i), ARGOF(b, i)))
            return 0;
      return SUCCESS;
   }
   return 0;
}


void unwind_trail(word* from)
{
   for (word* T = TR; T < from; T++)
   {
      *((Word)*T) = *T;
      //printf("Unbound variable %p\n", trail[i]);
   }
}


// After executing, cut_to(X), we ensure that X is the current choicepoint.
int cut_to(Choicepoint point)
{
   word* finalSP = SP;
   while (CP > point)
   {
      if (CP == NULL) // Fatal, I guess?
         fatal("Cut to non-existent choicepoint");
      Choicepoint c = CP;

      // If the choicepoint is further down the stack than the current frame then we can move SP all the way back to FR + fence
      // This implies that we should free any frames between FR and CP
      if (c->FR > FR)
      {
         Frame f = c->FR;
         while (f > FR)
         {
            if (f->is_local)
            {
               //printf("Freeing clause at %p\n", f->clause);
               free_clause(f->clause);
               f->is_local = 0;
            }
            f = f->parent;
         }
      }
      // We cannot move SP just yet since we may run a cleanup on this frame
      // and if we move SP back and start executing the cleanup goal now, it possibly write over
      // important information. Instead, note where we would have moved it back to, and only
      // move it once the cut is finalized
      if (c->FR > FR)
         finalSP = AFTER_FRAME(c->FR);

      CP = CP->CP;
      if (c->foreign_cleanup.fn != NULL)
      {
         // The PC stored in c->PC is the original PC + 3 + sizeof(word). We want to read out the value at slot [PC + 3 + sizeof(word)]
         word backtrack_ptr = c->FR->slots[CODE16(c->PC-2)];
         c->foreign_cleanup.fn(c->foreign_cleanup.arg, backtrack_ptr);
      }
      else if (c->cleanup != NULL)
      {
         Frame cleanupFrame = c->cleanup;
         if (unify(cleanupFrame->slots[1], cutAtom))
         {
            // call a prolog goal.
            PC--;
            ARGP = &cleanupFrame->slots[3]; // i_usercall looks at the slot before this one, which will be slot[2], holding the Cleanup goal
            // When the usercall returns we will end up back at the current PC, which means we will try to cut_to again.
            // However, next time, c->cleanup will be NULL and we will not run this cleanup again
            c->cleanup = NULL;
            return AGAIN;
         }
         c->cleanup = NULL; // Missed your chance then
      }
   }
   SP = finalSP;

   /*
   word* CPS = (word*)CP + sizeof(choicepoint)/sizeof(word) + (CP != NULL?CP->argc : 0);
   word* FRS = (word*)FR + FR->clause->slot_count + sizeof(frame) / sizeof(word);
   if (CPS > FRS)
   {
      //printf("Moving SP from %p down to the top of the next choicepoint: %p\n", SP, CPS);
      SP = CPS;
   }
   else
   {
      //printf("Moving SP from %p down to the top of the current frame: %p\n", SP, FRS);
      SP = FRS;
      NFR = (Frame)SP;
      ARGP = ARGS;
   }
   // We can now set SP to the maximum of:
   //    CP + sizeof(choicepoint)
   //    FR + fence
   */
   return 1;
}

int apply_choicepoint(Choicepoint c)
{
   PC = c->PC;
   H = c->H;
   Frame f = FR;
//   printf("Applying choicepoint at %p. FR is %p, choicepoint had frame %p\n", c, FR, c->FR);
   while (f > c->FR)
   {
//      printf("Checking for cleanup in frame %p\n", f);
      assert(f != f->parent);
      if (f->is_local)
      {
         //printf("Freeing clause from %p\n", f);
         free_clause(f->clause);
         f->is_local = 0;
      }
      f = f->parent;
   }
   FR = c->FR;
   NFR = c->NFR;
   SP = c->SP;
   CP = c->CP;
   TR = c->TR;
   currentModule = c->module;
   // We must not write to anything on FR (including FR->clause) until we have finished reading from the choicepoint, since FR may be on the stack
   // and is likely to be before the choicepoint, meaning that we will end up corrupting the choicepoint
   FR->clause = c->clause;
   if (c->type == Head)
   {
      mode = READ;
      // And we must now copy all the args back off the choicepoint
      //printf("Restoring %d args\n", c->argc);
      for (int i = 0; i < c->argc; i++)
      {
         ARGS[i] = c->args[i];
         //printf("%d: ", i); PORTRAY(ARGS[i]); printf("\n");
      }
      ARGP = ARGS;
      // Also we must ensure that there is sufficient space for the variables in this clause
      SP = AFTER_FRAME(FR);
   }
   else
   {
      ARGP = ARGS;
   }
   // We can start the arg stack again if we are trying again
   argStackP = &argStack[0];

   return (PC != NULL);
}

int backtrack()
{
   //printf("Backtracking...\n");
   word* oldTR = TR;
   while(1)
   {
      //printf("Backtracking %p\n", CP);
      if (CP == NULL)
      {
         // Complete failure. Undo all the bindings!
         TR = &TRAIL[0];
         unwind_trail(oldTR);      
         return 0;
      }
      if (!apply_choicepoint(CP))
         continue;
      //printf("unwinding %p\n", oldTR);
      unwind_trail(oldTR);
      return 1;
   }
}

int backtrack_to(Choicepoint c)
{
   // Undoes to the given choicepoint, undoing everything else in between
   while (CP != c)
   {
      word* oldTR = TR;
      if (CP == NULL)
      {
         TR = &TRAIL[0];
         unwind_trail(oldTR);
         return 0;
      }
      apply_choicepoint(CP); // Modifies CP
      unwind_trail(oldTR);
   }
   return 1;
}


word _copy_term(word t, List* variables, word* new_vars)
{
   if (TAGOF(t) == VARIABLE_TAG)
   {
      return new_vars[list_index(variables, t)];
   }
   else if (TAGOF(t) == COMPOUND_TAG)
   {
      Functor functor = getConstant(FUNCTOROF(t), NULL).functor_data;
      word new_args[functor->arity];
      for (int i = 0; i < functor->arity; i++)
         new_args[i] = _copy_term(ARGOF(t, i), variables, new_vars);
      return MAKE_ACOMPOUND(FUNCTOROF(t), new_args);
   }
   // For everything else, just return the term itself
   return t;
}

word copy_term(word term)
{
   term = DEREF(term);
   List variables;
   init_list(&variables);
   find_variables(term, &variables);
   int variable_count = list_length(&variables);
   word* new_vars = malloc(sizeof(word) * variable_count);
   for (int i = 0; i < variable_count; i++)
      new_vars[i] = MAKE_VAR();
   word result = _copy_term(term, &variables, new_vars);
   free(new_vars);
   free_list(&variables);
   return result;
}

int count_compounds(word t)
{
   t = DEREF(t);
   if (TAGOF(t) == COMPOUND_TAG)
   {
      int i = 1;
      Functor f = getConstant(FUNCTOROF(t), NULL).functor_data;
      for (int j = 0; j < f->arity; j++)
         i+=count_compounds(ARGOF(t, j));
      return i + f->arity;
   }
   return 0;
}

word make_local_(word t, List* variables, word* heap, int* ptr, int size)
{
   t = DEREF(t);
   switch(TAGOF(t))
   {
      case CONSTANT_TAG:
         return t;
      case POINTER_TAG:
         return t;
      case COMPOUND_TAG:
      {
         word result = (word)(&heap[*ptr]) | COMPOUND_TAG;
         heap[*ptr] = FUNCTOROF(t);
         (*ptr)++;
         int argp = *ptr;
         Functor f = getConstant(FUNCTOROF(t), NULL).functor_data;
         (*ptr) += f->arity;
         for (int i = 0; i < f->arity; i++)
         {
            heap[argp++] = make_local_(ARGOF(t, i), variables, heap, ptr, size);
         }
         return result;
      }
      case VARIABLE_TAG:
      {
         // The variables appear, backwards, at the end of the heap. This is so that
         // The term we want is also identifiable as (word)heap
         int i = list_index(variables, t);
         heap[size-i-1] = (word)&heap[size-i-1];
         return (word)&heap[size-i-1];
      }
   }
   assert(0);
}

word copy_local_with_extra_space(word t, word** local, int extra)
{
   t = DEREF(t);
   // First, if extra is 0 we can do some simple optimisations:
   if (extra == 0)
   {
      if (TAGOF(t) == CONSTANT_TAG || TAGOF(t) == POINTER_TAG)
      {
         *local = (word*)t;
         return t;
      }
      if (TAGOF(t) == VARIABLE_TAG)
      {
         word* localptr = malloc(sizeof(word));
         localptr[0] = (word)&localptr[0];
         return (word)&localptr[0];
      }
      // For compounds we must do the normal complicated process
   }


   List variables;
   init_list(&variables);
   int i = count_compounds(t);
   find_variables(t, &variables);
   i += list_length(&variables);
   i += extra;
   i++;
   word* localptr = malloc(sizeof(word) * i);
   assert(local != NULL);
   //printf("Allocated buffer for "); PORTRAY(t); printf(" at %d\n", (int)localptr);
   *local = localptr;

   int ptr = extra + 1;
   word w = make_local_(t, &variables, localptr, &ptr, i);
   localptr[extra] = w;
   free_list(&variables);
   return w;
}

EMSCRIPTEN_KEEPALIVE
word copy_local(word t, word** local)
{
   return copy_local_with_extra_space(t, local, 0);
}

void create_choicepoint(unsigned char* address, Clause clause, int type)
{
   //printf("Creating a choicepoint at %p with frame %p and continuation address %p\n", SP, FR, address);
   Choicepoint c = (Choicepoint)SP;
   c->SP = SP;
   c->CP = CP;
   c->H = H;
   CP = c;
   c->type = type;
   c->FR = FR;
   c->clause = clause;
   c->module = currentModule;
   c->cleanup = NULL;
   c->foreign_cleanup.fn = NULL;
   c->foreign_cleanup.arg = 0;
   c->NFR = NFR;
   c->functor = FR->functor;
   c->TR = TR;
   c->PC = address;
   if (type == Head)
   {
      // We must also copy all the arguments into the choicepoint here
      c->argc = getConstant(FR->functor, NULL).functor_data->arity;
//      printf("Saving %d args on the choicepoint\n", c->argc);
      for (int i = 0; i < c->argc; i++)
      {
//         printf("%d: ", i); PORTRAY(ARGS[i]); printf("\n");
         c->args[i] = ARGS[i];
      }
   }
   else
      c->argc = 0;
   SP = AFTER_CHOICE(c);
}


Choicepoint push_state()
{
   //printf("Pushing state. CP is %p, PC is %p, FR is %p, and frame locality is %d, ", CP, PC, FR, FR->is_local);
   create_choicepoint(PC, FR->clause, Body);
   assert(FR->is_local >= 0 && FR->is_local <= 1);
   Choicepoint state = CP;
   //printf("you can get back here by restoring %p\n", state);
   CP = 0;
   //printf("State is now %p\n", state);
   return state;
}

void restore_state(Choicepoint state)
{
   //printf("Popping state from %p\n", state);
   //printf("Restoring state to %p from the current CP of %p\n", state, CP);
   //print_choices();
   // First though, we must cut any choicepoints that might be lurking on the stack
   cut_to(0);
   CP = state;
   //printf("Restoring state. from %p. CP is %p, PC is %p, FR is %p, and frame locality is %d\n", CP, CP->CP, CP->PC, CP->FR, CP->FR->is_local);
   apply_choicepoint(CP);
   assert(FR->is_local >= 0 && FR->is_local <= 1);
   //printf("Done. PC is now %p\n", PC);
}

uint8_t failOp = I_FAIL;
clause failClause = {NULL, &failOp, NULL, 1, 0};

uint8_t exitQueryOp = I_EXIT_QUERY;
clause exitQueryClause = {NULL, &exitQueryOp, NULL, 1, 0};


Clause get_predicate_code(Predicate p)
{
   if (p->firstClause == NULL)
      p->firstClause = compile_predicate(p);
   return p->firstClause;
}

// prepare_frame fills in all the fields of the frame that make sense.
// You must still fill in returnPC, and in the case of I_DEPART, parent
int prepare_frame(word functor, Module optionalContext, Frame frame, Frame parent)
{
   Module module = (optionalContext != NULL)?optionalContext:currentModule;
   Predicate p = lookup_predicate(module, functor);
   if (p == NULL && module != userModule)
   {
      //printf("Trying again in user\n");
      // try again in module(user)
      p = lookup_predicate(userModule, functor);
      if (p != NULL)
      {
         currentModule = userModule;
         frame->contextModule = userModule;
      }
   }
   else if (p != NULL)
   {
      frame->contextModule = module;
   }

   if (p == NULL)
   {
      Functor f = getConstant(functor, NULL).functor_data;
      if (get_prolog_flag("unknown") == errorAtom)
      {
         //SET_EXCEPTION(procedureAtom);
         return existence_error(procedureAtom, MAKE_VCOMPOUND(predicateIndicatorFunctor, f->name, MAKE_INTEGER(f->arity)));
         return 0;
      }
      else if (get_prolog_flag("unknown") == failAtom)
      {
         frame->clause = &failClause;
         frame->functor = failFunctor;
      }
      else if (get_prolog_flag("unknown") == warningAtom)
      {
         printf("Warning from the ISO committee: No such predicate "); PORTRAY(f->name); printf("/%d\n", f->arity);
         frame->clause = &failClause;
         frame->functor = failFunctor;
      }
   }
   else
   {
      //printf("Functor of %p is set to ", frame); PORTRAY(functor); printf("\n");
      frame->functor = functor;
      frame->clause = get_predicate_code(p);
      if (frame->clause == NULL)
      {
         return 0;
      }
   }
   frame->is_local = 0;
   assert(frame != parent);
   frame->parent = parent;
   //printf("    parent of frame is %p\n", parent);
   if (FR != NULL)
      frame->depth = FR->depth+1;
   else
      frame->depth = 0;
   frame->choicepoint = CP;
   return 1;
}

Frame allocFrame(Frame parent)
{
   //printf("Making a frame at %p\n", SP);
   Frame f = (Frame)SP;
   SP += sizeof(frame)/sizeof(word);
   //printf("frame extends until %p\n", SP);
   assert(f != parent);
   f->parent = parent;
   if (FR != NULL)
      f->depth = FR->depth+1;
   else
      f->depth = 0;
   f->clause = NULL;
   f->contextModule = currentModule;
   f->returnPC = 0;
   f->choicepoint = CP;
   f->is_local = 0;
   return f;
}

void initialize_kernel()
{
   userModule = create_module(MAKE_ATOM("user"));
   currentModule = userModule;
   current_input = nullStream();
   current_output = consoleOuputStream();
   initialize_foreign();
   consult_string_of_length((char*)src_builtin_pl, src_builtin_pl_len);
   PC = 0;
   CP = 0;
   // Allocate a dummy-frame at the very top of the stack. This allows us to call push_state() if desired even when there is
   // no current frame.
   FR = allocFrame(NULL);
   FR->functor = MAKE_FUNCTOR(MAKE_ATOM("<system>"), 1);
}

void hard_reset()
{
   destroy_module(userModule);
   initialize_kernel();
}


RC execute(int resume)
{
   if (current_exception != 0)
      goto b_throw_foreign;
   else if (resume)
      goto i_exit;
   while (!halted)
   {
      //print_choices();
      //ctable_check();
      if (debugging)
         print_instruction();
      switch(*PC)
      {
         case I_FAIL:
            // Simple enough - fail
            if (backtrack())
               continue;
            return FAIL;
         case I_ENTER:
            // I_ENTER happens after we have finished matching the head. At this point, all we need to do is move ARGP back to ARGS.
            // The frame will already have been fully formed, either by the I_CALL/I_DEPART that got us here, or by TRY_ME_OR_NEXT_CLAUSE
            ARGP = ARGS;
            PC++;
            continue;
         case I_EXIT_QUERY:
            // Exit from the toplevel
            if (CP == initialChoicepoint)
               return SUCCESS;
            return SUCCESS_WITH_CHOICES;
         case I_EXITCATCH:
            // If there was no (additional) choicepoint created by the catch/3 goal then we can remove the (fake) one created by I_CATCH
            if (FR->slots[CODE16(PC+1)] == (word)CP)
               CP = CP->CP;
            goto i_exit;
         case I_FOREIGN:
         {
            // A call to a deterministic, native (ie C) predicate
            RC rc = FAIL;
            ExecutionCallback yp = current_yield_ptr; // We must save this in case the foreign predicate trashes it
            Functor f = getConstant(FR->functor, NULL).functor_data;
            switch(f->arity)
            {
               case 0: rc = ((int (*)())((word)(CODEPTR(PC+1))))(); break;
               case 1: rc = ((int (*)(word))((word)(CODEPTR(PC+1))))(DEREF(*ARGP)); break;
               case 2: rc = ((int (*)(word,word))((word)(CODEPTR(PC+1))))(DEREF(*ARGP), DEREF(*(ARGP+1))); break;
               case 3: rc = ((int (*)(word,word,word))((word)(CODEPTR(PC+1))))(DEREF(*ARGP), DEREF(*(ARGP+1)), DEREF(*(ARGP+2))); break;
               case 4: rc = ((int (*)(word,word,word,word))((word)(CODEPTR(PC+1))))(DEREF(*ARGP), DEREF(*(ARGP+1)), DEREF(*(ARGP+2)), DEREF(*(ARGP+3))); break;
               case 5: rc = ((int (*)(word,word,word,word,word))((word)(CODEPTR(PC+1))))(DEREF(*ARGP), DEREF(*(ARGP+1)), DEREF(*(ARGP+2)), DEREF(*(ARGP+3)), DEREF(*(ARGP+4))); break;
               case 6: rc = ((int (*)(word,word,word,word,word,word))((word)(CODEPTR(PC+1))))(DEREF(*ARGP), DEREF(*(ARGP+1)), DEREF(*(ARGP+2)), DEREF(*(ARGP+3)), DEREF(*(ARGP+4)), DEREF(*(ARGP+5))); break;
               case 7: rc = ((int (*)(word,word,word,word,word,word,word))((word)(CODEPTR(PC+1))))(DEREF(*ARGP), DEREF(*(ARGP+1)), DEREF(*(ARGP+2)), DEREF(*(ARGP+3)), DEREF(*(ARGP+4)), DEREF(*(ARGP+5)), DEREF(*(ARGP+6))); break;
               case 8: rc = ((int (*)(word,word,word,word,word,word,word,word))((word)(CODEPTR(PC+1))))(DEREF(*ARGP), DEREF(*(ARGP+1)), DEREF(*(ARGP+2)), DEREF(*(ARGP+3)), DEREF(*(ARGP+4)), DEREF(*(ARGP+5)), DEREF(*(ARGP+6)), DEREF(*(ARGP+7))); break;
               case 9: rc = ((int (*)(word,word,word,word,word,word,word,word,word))((word)(CODEPTR(PC+1))))(DEREF(*ARGP), DEREF(*(ARGP+1)), DEREF(*(ARGP+2)), DEREF(*(ARGP+3)), DEREF(*(ARGP+4)), DEREF(*(ARGP+5)), DEREF(*(ARGP+6)), DEREF(*(ARGP+7)), DEREF(*(ARGP+8))); break;
               default:
                  // Too many args! This should be impossible since the installer would have rejected it
                  rc = SET_EXCEPTION(existence_error(procedureAtom, MAKE_VCOMPOUND(predicateIndicatorFunctor, f->name, MAKE_INTEGER(f->arity))));
            }
            current_yield_ptr = yp; // Restore the yield pointer
            // Now handle the consequences. If the exception is non-zero, then assume that the predicate raised it
            if (current_exception != 0)
               goto b_throw_foreign;
            if (rc == FAIL)         // Failure
            {
               if (backtrack())
                  continue;
               return FAIL;
            }
            else if (rc == SUCCESS) // Success (deterministic)
               goto i_exit;
            else if (rc == YIELD)   // Yield (just exit immediately)
               return YIELD;
            else if (rc == ERROR)   // If the predicate explicitly returns ERROR then we also accept that
               goto b_throw_foreign;
         }
         case I_FOREIGN_NONDET:
         case I_FOREIGN_JS:
            // A call to a nondeterministic foreign predicate. This includes all Javascript predicates - they are all assumed to be nondet.
            FR->slots[CODE16(PC+1+sizeof(word))] = (word)0;
            // We have to move PC forward since the retry step that we fall-through just below will rewind it
            PC += (3+sizeof(word));
            // fall-through
         case I_FOREIGN_JS_RETRY:
         case I_FOREIGN_RETRY:
         {
            RC rc = FAIL;
            Functor f = getConstant(FR->functor, NULL).functor_data;
            ExecutionCallback yp = current_yield_ptr; // We must save this in case the foreign predicate trashes it
            if (*PC == I_FOREIGN_RETRY || *PC == I_FOREIGN_NONDET) // Native (ie C)
            {
               // Go back to the actual FOREIGN_NONDET or FOREIGN_JS call
               PC -= (3+sizeof(word));
               unsigned char* foreign_ptr = PC+1+sizeof(word);
               switch(f->arity)
               {
                  case 0: rc = ((int (*)(word))((word)(CODEPTR(PC+1))))(FR->slots[CODE16(foreign_ptr)]); break;
                  case 1: rc = ((int (*)(word,word))((word)(CODEPTR(PC+1))))(DEREF(*ARGP), FR->slots[CODE16(foreign_ptr)]); break;
                  case 2: rc = ((int (*)(word,word,word))((word)(CODEPTR(PC+1))))(DEREF(*ARGP), DEREF(*(ARGP+1)), FR->slots[CODE16(foreign_ptr)]); break;
                  case 3: rc = ((int (*)(word,word,word,word))((word)(CODEPTR(PC+1))))(DEREF(*ARGP), DEREF(*(ARGP+1)), DEREF(*(ARGP+2)), FR->slots[CODE16(foreign_ptr)]); break;
                  case 4: rc = ((int (*)(word,word,word,word,word))((word)(CODEPTR(PC+1))))(DEREF(*ARGP), DEREF(*(ARGP+1)), DEREF(*(ARGP+2)), DEREF(*(ARGP+3)), FR->slots[CODE16(foreign_ptr)]); break;
                  case 5: rc = ((int (*)(word,word,word,word,word,word))((word)(CODEPTR(PC+1))))(DEREF(*ARGP), DEREF(*(ARGP+1)), DEREF(*(ARGP+2)), DEREF(*(ARGP+3)), DEREF(*(ARGP+4)), FR->slots[CODE16(foreign_ptr)]); break;
                  case 6: rc = ((int (*)(word,word,word,word,word,word,word))((word)(CODEPTR(PC+1))))(DEREF(*ARGP), DEREF(*(ARGP+1)), DEREF(*(ARGP+2)), DEREF(*(ARGP+3)), DEREF(*(ARGP+4)), DEREF(*(ARGP+5)), FR->slots[CODE16(foreign_ptr)]); break;
                  case 7: rc = ((int (*)(word,word,word,word,word,word,word,word))((word)(CODEPTR(PC+1))))(DEREF(*ARGP), DEREF(*(ARGP+1)), DEREF(*(ARGP+2)), DEREF(*(ARGP+3)), DEREF(*(ARGP+4)), DEREF(*(ARGP+5)), DEREF(*(ARGP+6)), FR->slots[CODE16(foreign_ptr)]); break;
                  case 8: rc = ((int (*)(word,word,word,word,word,word,word,word,word))((word)(CODEPTR(PC+1))))(DEREF(*ARGP), DEREF(*(ARGP+1)), DEREF(*(ARGP+2)), DEREF(*(ARGP+3)), DEREF(*(ARGP+4)), DEREF(*(ARGP+5)), DEREF(*(ARGP+6)), DEREF(*(ARGP+7)), FR->slots[CODE16(foreign_ptr)]); break;
                  case 9: rc = ((int (*)(word,word,word,word,word,word,word,word,word,word))((word)(CODEPTR(PC+1))))(DEREF(*ARGP), DEREF(*(ARGP+1)), DEREF(*(ARGP+2)), DEREF(*(ARGP+3)), DEREF(*(ARGP+4)), DEREF(*(ARGP+5)), DEREF(*(ARGP+6)), DEREF(*(ARGP+7)), DEREF(*(ARGP+8)), FR->slots[CODE16(foreign_ptr)]); break;
                  default:
                     // Too many args! This should be impossible since the installer would have rejected it
                     rc = SET_EXCEPTION(existence_error(procedureAtom, MAKE_VCOMPOUND(predicateIndicatorFunctor, f->name, MAKE_INTEGER(f->arity))));
               }
            }
            else  // Javascript call
            {
#ifdef EMSCRIPTEN
               PC -= (3+sizeof(word));
               unsigned char* foreign_ptr = PC+1+sizeof(word);

               rc = EM_ASM_INT({return _foreign_call($0, $1, $2, $3)}, FR->slots[CODE16(foreign_ptr)], CODEPTR(PC+1), f->arity, ARGP);
#else
               rc = SET_EXCEPTION(existence_error(procedureAtom, MAKE_VCOMPOUND(predicateIndicatorFunctor, f->name, MAKE_INTEGER(f->arity))));
#endif
            }
            current_yield_ptr = yp;
            // Handle the consequences. See the comments in I_FOREIGN
            if (current_exception != 0)
               goto b_throw_foreign;
            if (rc == FAIL)
            {
               if (backtrack())
                  continue;
               return FAIL;
            }
            else if (rc == SUCCESS)
               goto i_exit;
            else if (rc == YIELD)
               return YIELD;
            else if (rc == ERROR)
               goto b_throw_foreign;
         }
         i_exit:
         case I_EXIT:
         case I_EXIT_FACT:
         {
            // Exit from the current frame. We need to hop back to the parent frame and continue execution from there
            // If, as we move the frame pointer back to the parent, we don't pass any choicepoints, we can trim the stack back to that point
            // If we pass a choicepoint, we can trim the stack to there
            if ((uintptr_t)CP < (uintptr_t)FR->parent)
            {
               // No choicepoints. Move SP back to just after the parent frame
               SP = AFTER_FRAME(FR->parent);
            }
            else
            {
               // Choicepoint exists. We can only wind SP back as far as that
               SP = AFTER_CHOICE(CP);
            }
            PC = FR->returnPC;
            if (FR->is_local && (word)FR >= (word)SP)
            {
               // Free the clause if it was generated specifically for this frame and it is now above SP (ie 'free')
               // FIXME: There could be more than one such clause, couldnt there?
               free_clause(FR->clause);
            }
            FR = FR->parent;
            NFR = (Frame)SP;
            // Now we are about to start preparing the next subgoal, so set ARGP back to ARGS
            ARGP = ARGS;
            continue;
         }
         case I_DEPART:
         {
            // I_DEPART is like I_CALL except it destroys the current frame
            word functor = FR->clause->constants[CODE16(PC+1)];
            // We need to save some things which are about to be clobbered since the current frame gets overwritten
            unsigned char* returnPC = FR->returnPC;
            Frame parent = FR->parent;
            assert(parent != FR);
            Module contextModule = FR->contextModule;
            if (FR->is_local && (word)FR >= (word)CP)
            {
               // Free the clause if it was generated specifically for this frame and we cannot possibly backtrack into it
               free_clause(FR->clause);
            }

            // Now we do what we would have otherwise done in I_EXIT, unwinding the frame
            if ((uintptr_t)CP < (uintptr_t)FR->parent)
            {
               // No choicepoints. Move SP back to just after the parent frame
               SP = AFTER_FRAME(FR->parent);
            }
            else
            {
               // Choicepoint exists. We can only wind SP back as far as that
               SP = AFTER_CHOICE(CP);
            }
            NFR = (Frame)SP; // If there are no choicepoints, NFR will equal FR. Otherwise, NFR will be after the choicepoint.
            // From this point on, the old frame is possibly going to be scribbled all over. Do not try to use it!
            if (!prepare_frame(functor, contextModule, NFR, parent))
               goto b_throw_foreign;
            SP = AFTER_FRAME(NFR);
            NFR->returnPC = returnPC;
            FR = NFR;
            ARGP = ARGS;
            // FR is now the new frame. Set up the variables as needed
            FR->choicepoint = CP;
            PC = FR->clause->code;
            /*
            for (int i = 0; i < getConstant(FR->functor, NULL).functor_data->arity; i++)
            {
               printf("Arg %d: %08lx = ", i, ARGS[i]); PORTRAY(ARGS[i]); printf("\n");
            }
            */
            continue;
         }
         case B_CLEANUP_CHOICEPOINT:
         {
            // This just leaves a fake choicepoint (one you cannot backtrack onto) with a .cleanup value set to the cleanup handler
            create_choicepoint(NULL, FR->clause, Body);
            CP->cleanup = FR;
            PC++;
            continue;
         }
         case B_THROW:
            //printf("Setting exception from: "); PORTRAY(*(ARGP-1)); printf("\n");
            SET_EXCEPTION(*(ARGP-1));
            // fall-through
         case B_THROW_FOREIGN:
         b_throw_foreign:
         {
            assert(current_exception != 0 && "throw but no exception set?");
            // We build up a backtrace regardless of whether we actually need it
            List backtrace;
            init_list(&backtrace);
            Frame f = FR;
            while(f != NULL)
            {
               //printf("Checking for catch/3 in %p\n", f); PORTRAY(f->functor); printf("\n");
               list_append(&backtrace, f->functor);
               if (f->functor == catchFunctor)
               {
                  //printf("Found a catch/3 in %p with choicepoint %p\n", f, f->choicepoint);
                  //printf("SP is now %p\n", SP);
                  backtrack_to(f->choicepoint);

                  // And then undo the fake choicepoint. Note that this resets H destroying all terms created since the catch was called
                  apply_choicepoint(CP);
                  if (unify(copy_term(current_exception), f->slots[1]))
                  {
                     // Success! Exception is successfully handled. First unify the backtrace if possible (ignoring it if not)
                     if (TAGOF(current_exception) == COMPOUND_TAG && FUNCTOROF(current_exception) == errorFunctor && (TAGOF(ARGOF(current_exception, 1)) == VARIABLE_TAG))
                     {
                        unify(ARGOF(DEREF(f->slots[1]), 1), make_backtrace(&backtrace));
                     }
                     free_list(&backtrace);
                     CLEAR_EXCEPTION();

                     // Now we just have to do i_usercall to execute the handler. To do that, we must first adjust the state so we are pointing to it
                     // Things get a bit weird here because we are breaking the usual logic flow by ripping the VM out of whatever state it was in and starting
                     // it off in a totally different place. We have to reset ARGP and PC then pretend the next instruction was i_usercall
                     // ARGP is going to point to the third variable in the frame, which is unusual (it usually points to the heap or ARGS)
                     ARGP = f->slots + 3;
                     PC = &f->clause->code[12];
                     // We also invalidate the functor so that we cannot unify with this catcher frame again
                     f->functor = caughtFunctor;
                     // Adjust for USERCALL so it sets up the right return address - we want to pretend this is where we are current executing
                     PC = f->returnPC-1;
                     FR = f->parent;
                     goto i_usercall;
                  }
               }
               f = f->parent;
            }
            // Failed to handle. Return to top level. But first, try to unify the stack trace with the actual error term (and not a copy)
            if (TAGOF(current_exception) == COMPOUND_TAG && FUNCTOROF(current_exception) == errorFunctor && (TAGOF(ARGOF(current_exception, 1)) == VARIABLE_TAG))
            {
               unify(ARGOF(current_exception, 1), make_backtrace(&backtrace));
            }
            // Remember to push FR back otherwise we might end up trying to clean up (now discarded) frames
            FR = f;
            free_list(&backtrace);
            return ERROR;
         }
         case I_SWITCH_MODULE:
         {
            // This just changes the current module
            word moduleName = FR->clause->constants[CODE16(PC+1)];
            currentModule = find_module(moduleName);
            FR->contextModule = currentModule;
            PC+=3;
            continue;
         }
         case I_EXITMODULE:
         {
            // Current this is a NOP
            PC++;
            continue;
         }
         case I_CALL:
         {
            // I_CALL creates a new frame at SP (later, at SP - N, where N is the number of slots we can reclaim from this frame for environment trimming)
            NFR = (Frame)SP;
            word functor = FR->clause->constants[CODE16(PC+1)];
            assert((word*)NFR < STOP);  // Make sure there is space!
            // Build the frame
            if (!prepare_frame(functor, FR->contextModule, NFR, FR))
               goto b_throw_foreign;
            NFR->returnPC = PC+3;
            ARGP = ARGS;
            FR = NFR;
            // We must also ensure that there is enough space. prepare_frame does not push up SP
            SP = AFTER_FRAME(FR);
            FR->choicepoint = CP;
            PC = FR->clause->code;
            /*
            for (int i = 0; i < getConstant(FR->functor, NULL).functor_data->arity; i++)
            {
               printf("Arg %d: ", i); PORTRAY(ARGS[i]); printf("\n");
            }
            */
            continue;
         }
         case I_CATCH:
         {
            // I_CATCH creates a (fake) choicepoint then goes straight to i_usercall
            create_choicepoint(0, FR->clause, Head);
            FR->choicepoint = CP;
            FR->slots[CODE16(PC+1)] = (word)CP;
            // ARGP is set to slot 1 so that we try and execute the first arg as a goal. (I_USERCALL subtracts a word from ARGP, so we actually execute slot[0])
            ARGP = &FR->slots[1];
            // printf("Slots[0] is %08lx\n", FR->slots[0]); PORTRAY(FR->slots[0]); printf("\n");
            PC+=2; // i_usercall returns to PC+1, and we want to return to PC+3, so add 2 here
            goto i_usercall;
         }
         case I_USERCALL:
         i_usercall:
         {
            word goal = DEREF(*(ARGP-1));
            //printf("i_usercall of arg at %p\n   ", ARGP-1); PORTRAY(goal);printf("\n");
            if (TAGOF(goal) == VARIABLE_TAG)
            {
               instantiation_error();
               goto b_throw_foreign;
            }
            Query query = compile_query(goal);
            if (query == NULL)
               goto b_throw_foreign;
            NFR = (Frame)SP;
            assert(NFR != FR);
            NFR->parent = FR;
            NFR->functor = MAKE_FUNCTOR(MAKE_ATOM("<meta-call>"), 1);
            NFR->clause = query->clause;
            //printf("Allocated frame %p with local clause %p\n", NFR, NFR->clause);
            NFR->contextModule = NULL; // CHECKME: Is this right?
            NFR->is_local = 1;
            NFR->returnPC = PC+1;
            NFR->choicepoint = CP;
            ARGP = ARGS;
            for (int i = 0; i < query->variable_count; i++)
            {
               //printf("Populating meta-call arg %d at location %p\n", i, &NFR->slots[i]);
               ARGS[i] = query->variables[i];
            }
            FR = NFR;
            SP = AFTER_FRAME(FR);
            PC = FR->clause->code;
            free_query(query);
            continue;
         }
         case I_CUT:
         {
            // I_CUT is a normal body cut. We just have to cut to the choicepoint that was around when the frame was created
            RC rc = cut_to(FR->choicepoint);
            if (rc == YIELD)
               return YIELD;
            else if (rc == AGAIN) // We get AGAIN if we had to stop cutting so we could execute a cleanup
               goto i_usercall;
            // Update the choicepoint in the frame (FIXME: Isn't this a NOP?)
            assert(CP == FR->choicepoint);
            ARGP = ARGS;
            PC++;
            continue;
         }
         case C_CUT:
         {
            // C_CUT is the cut you get in an if-then-else. We must cut to the /given/ choicepoint (which was saved as part of IF-THEN or IF-THEN-ELSE)
            RC rc = cut_to(((Choicepoint)FR->slots[CODE16(PC+1)]));
            if (rc == YIELD)
               return YIELD;
            if (rc == AGAIN) // We get AGAIN if we had to stop cutting so we could execute a cleanup
               goto i_usercall;
            PC+=3;
            continue;
         }
         case C_LCUT:
         {
            // C_LCUT is a cut in the if-part of an if-then-else. We must cut to the choicepoint AFTER the given one.
            Choicepoint C = CP;
            int found = 0;
            while (C > ((Choicepoint)FR->slots[CODE16(PC+1)]))
            {
               assert(C != NULL); // The choicepoint we are looking for is gone
               if (C->CP == ((Choicepoint)FR->slots[CODE16(PC+1)]))
               {
                  // Good, we found it
                  RC rc = cut_to(C);
                  if (rc == YIELD)
                     return YIELD;
                  if (rc == AGAIN)
                     goto i_usercall;
                  PC+=3;
                  found = 1;
                  break;
               }
            }
            assert(found);
            continue;
         }
         case C_IF_THEN:
         {
            // C_IF_THEN just has to save the current choicepoint so we can cut it later with C_CUT
            FR->slots[CODE16(PC+1)] = (word)CP;
            PC+=3;
            continue;
         }
         case C_IF_THEN_ELSE:
         {
            // C_IF_THEN has to save the current choicepoint so we can cut it later with C_CUT, then also create a new choicepoint
            // to try the ELSE part if the IF part fails
            FR->slots[CODE16(PC+1+sizeof(word))] = (word)CP;
            unsigned char* address = PC+CODEPTR(PC+1);
            create_choicepoint(address, FR->clause, Body);
            PC+=3 + sizeof(word);
            continue;
         }
         case I_UNIFY:
         {
            // I_UNIFY just tries to unify 2 arguments
            word t1 = *(ARGP-1);
            word t2 = *(ARGP-2);
            ARGP-=2;
            if (unify(t1, t2))
            {
               PC++;
               continue;
            }
            else if (backtrack())
               continue;
            return FAIL;
         }
         case B_FIRSTVAR:
            // This is a variable which we have not seen before (ie was not present in the head) and is being used now for the first time
            // We can make a local variable on the frame, since if this were unsafe we would have instead gotten B_UNSAFEVAR
            FR->slots[CODE16(PC+1)] = (word)&FR->slots[CODE16(PC+1)];
            *(ARGP++) = FR->slots[CODE16(PC+1)];
            //printf("Slot %d now holds %08lx, a local variable\n", CODE16(PC+1), FR->slots[CODE16(PC+1)]);
            PC+=3;
            continue;
         case B_ARGFIRSTVAR:
            // B_ARGFIRSTVAR is a variable which we have not seen before and is now being used for the first time as an argument of a compound term
            // We cannot just make a local variable here since ARGP is pointing to the heap, and that means it could live on well past the lifetime of
            // this frame. Instead, make a variable on the heap and set our slot to point to that.
            (*ARGP) = MAKE_VAR();
            FR->slots[CODE16(PC+1)] = *ARGP;
            ARGP++;
            PC+=3;
            continue;
         case B_ARGVAR:
         {
            // B_ARGVAR is a variable that we have already initialized and is now being used as the argument of a compound term.
            unsigned int slot = CODE16(PC+1);
            word arg = DEREF(FR->slots[slot]);
            // If the slot is already bound, just write the value into the heap cell
            if (TAGOF(arg) != VARIABLE_TAG)
            {
               *(ARGP++) = arg;
            }
            else
            {
               // Otherwise, we may have a problem. If the variable is on the heap we can just write it directly to ARPG, but if it is local
               // then we need to globalize it since otherwise we would be writing a local variable to the heap
               if (arg > (word)STACK)
               {
                  *ARGP = MAKE_VAR();
                  _bind(arg, *(ARGP++));
               }
               else
                  *(ARGP++) = arg;
            }
            PC+=3;
            continue;
         }
         case B_VAR:
         {
            // B_VAR is a variable that we have already initialized and is now being used as the argument of a subgoal
            // It is always safe to just copy the value. If the variable were about to be trimmed from the local environment
            // then instead we would get B_UNSAFEVAR
            unsigned int slot = CODE16(PC+1);
            assert (FR->slots[slot] != 0 && "Suspicious lack of variable initialization!");
            //printf("Value: %08lx\n   ", FR->slots[slot]); PORTRAY(FR->slots[slot]);printf(" written to %p\n", ARGP);
            *(ARGP++) = FR->slots[slot];
            PC+=3;
            continue;
         }
         case B_FIRSTUNSAFEVAR:
         {
            // B_FIRSTUNSAFEVAR is a variable that we are seeing for the first time but is unsafe (ie this is also the last time we are
            // going to see it). For example, foo(X, a(X))
            // We know it is still a variable because it is fresh, but we also know we must allocate it on the heap since the frame is
            // about to be destroyed
            unsigned int slot = CODE16(PC+1);
            FR->slots[slot] = MAKE_VAR();
            *(ARGP++) = FR->slots[slot];
            PC+=3;
            continue;
         }
         case B_UNSAFEVAR:
         {
            // B_UNSAFEVAR is a variable that we have already initialized but is about to be trimmed from the environment
            // If it is already bound, then that is OK - just copy the value
            // If it is a variable, but on the heap - that is also OK
            // Otherwise we must globalize it as with B_ARGVAR
            unsigned int slot = CODE16(PC+1);
            word arg = DEREF(FR->slots[slot]);
            if (TAGOF(arg) != VARIABLE_TAG)
            {
               *(ARGP++) = arg;
            }
            else
            {
               if (arg > (word)STACK)
               {
                  *ARGP = MAKE_VAR();
                  _bind(FR->slots[slot], *ARGP);
                  ARGP++;
               }
               else
                  *(ARGP++) = arg;
            }
            PC+=3;
            continue;
         }
         case B_POP:
         {
            // This just restores ARGP
            ARGP = (word*)*(--argStackP);
            PC++;
            continue;
         }
         case B_ATOM:
         {
            // Put an atom
            *(ARGP++) = FR->clause->constants[CODE16(PC+1)];
            PC+=3;
            continue;
         }
         case B_FUNCTOR:
         {
            // Put a reference to a compound. Move ARGP to the first arg of that term
            word t = MAKE_COMPOUND(FR->clause->constants[CODE16(PC+1)]);
            *(ARGP++) = t;
            assert(argStackP < argStackTop);
            *(argStackP++) = (uintptr_t)ARGP;
            ARGP = ARGPOF(t);
            PC+=3;
            continue;
         }
         case B_RFUNCTOR:
         {
            // Like B_FUNCTOR except it does not need to push to the argStack since it is the last arg in the word
            // When we do the B_POP for the last non-rfunctor we will reset ARGP to the next place in the list
            word t = MAKE_COMPOUND(FR->clause->constants[CODE16(PC+1)]);
            *(ARGP++) = t;
            ARGP = ARGPOF(t);
            PC+=3;
            continue;
         }
         case H_FIRSTVAR:
         {
            // ARGP is pointing to something we must match with a variable in the head that we have not seen until now (and is not an arg)
            // It will necessarily be on the heap, however we do not need to trail so long as we make sure FR->slots[CODE16(PC+1)] points to
            // ARGP, and not the other way around.
            if (mode == WRITE)
            {
               // We are writing, so first make ARGP (which will be one of the args of the term we are constructing on the heap) into a fresh var
               *ARGP = MAKE_VAR();
               // Then make a our frame variable a pointer to that var so we link them together
               FR->slots[CODE16(PC+1)] = (word)ARGP;
               ARGP++;
            }
            else
            {
               // We are reading (ie matching), so ARGP may point to either a variable, in which case we just make a pointer to it in our frame
               // or else something non-var, in which case we can just copy it into the frame
               if (TAGOF(*ARGP) == VARIABLE_TAG)
               {
                  //printf("Value: %08lx from %p\n   ", *ARGP, ARGP); PORTRAY(*ARGP);printf("\n");
                  word w = *(ARGP++);
                  FR->slots[CODE16(PC+1)] = w;
                  // The variable must either be on the heap (safe) or have been made local via a call to make_local (risky - you can free this memory later and
                  // end up with a pointer to nowhere)
                  assert(w < (word)FR || w > (word)TTOP);
               }
               else
               {
                  //printf("Bound value: %08lx from %p\n    ", *ARGP, ARGP); PORTRAY(*ARGP); printf("\n");
                  FR->slots[CODE16(PC+1)] = *(ARGP++);
               }
            }
            PC+=3;
            continue;
         }
         case H_FUNCTOR:
         {
            // Matching a functor. We switch to write mode if ARGP is a variable.
            word functor = FR->clause->constants[CODE16(PC+1)];
            word arg = DEREF(*(ARGP++));
            PC+=3;
            if (TAGOF(arg) == COMPOUND_TAG)
            {
               if (FUNCTOROF(arg) == functor)
               {
                  assert(argStackP < argStackTop);
                  *(argStackP++) = (uintptr_t)ARGP | mode;
                  ARGP = ARGPOF(arg);
                  continue;
               }
            }
            else if (TAGOF(arg) == VARIABLE_TAG)
            {
               assert(argStackP < argStackTop);
               *(argStackP++) = (uintptr_t)ARGP | mode;
               word t = MAKE_COMPOUND(functor);
               _bind(arg, t);
               ARGP = ARGPOF(t);
               mode = WRITE;
               continue;
            }
            if (backtrack()) // Failed to match
            {
               continue;
            }
            return FAIL;
         }
         case H_POP:
         {
            // H_POP just restores ARGP after a detour to a compound
            uintptr_t t = *--argStackP;
            mode = t & 1;
            ARGP = (word*)(t & ~1);
            PC++;
            continue;
         }
         case H_ATOM:
         {
            // H_ATOM tries to match an atom in the head. If the target is a variable, then we bind.
            word atom = FR->clause->constants[CODE16(PC+1)];
            word arg = DEREF(*(ARGP++));
            PC+=3;
            if (arg == atom)
               continue;
            else if (TAGOF(arg) == VARIABLE_TAG)
            {
               _bind(arg, atom);
               continue;
            }
            else
            {
               if (backtrack())
               continue;
            }
            return FAIL;
         }
         case H_VOID:
            assert(0 && "This should never be executed since the LCO change");
            FR->slots[CODE16(PC+1)] = (word)ARGP;
            ARGP++;
            PC++;
            continue;
         case H_VAR:
         {
            // H_VAR is a variable we have seen before. Just use general-purpose unification
            if (!unify(DEREF(*(ARGP++)), FR->slots[CODE16(PC+1)]))
            {
               if (backtrack())
                  continue;
               return FAIL;
            }
            PC+=3;
            continue;
         }
         case C_JUMP:
         {
            // C_JUMP is just a jump. Used by control structures like IF_THEN_ELSE
            PC += CODEPTR(PC+1);
            continue;
         }
         case C_OR:
         {
            // C_OR creates a 'Body' choicepoint
            create_choicepoint(PC+CODEPTR(PC+1), FR->clause, Body);
            PC += 1 + sizeof(word);
            continue;
         }
         case TRY_ME_OR_NEXT_CLAUSE:
         {
            // TRY_ME_OR_NEXT_CLAUSE creates a 'Head' choicepoint.
            create_choicepoint(FR->clause->next->code, FR->clause->next, Head);
            PC++;
            continue;
         }
         case TRUST_ME:
         {
            // This is currently a NOP
            PC++;
            continue;
         }
         case S_QUALIFY:
         {
            // S_QUALIFY turns the nth argument from X -> Module:X
            // Used by meta-predicates
            unsigned int slot = CODE16(PC+1);
            word value = DEREF(ARGS[slot]);
            if (!(TAGOF(value) == COMPOUND_TAG && FUNCTOROF(value) == crossModuleCallFunctor))
               ARGS[slot] = MAKE_VCOMPOUND(crossModuleCallFunctor, FR->parent->contextModule->name, value);
            //printf("Arg %d is now ", slot); PORTRAY(ARGS[slot]); printf("\n");
            PC+=3;
            continue;
         }
         case C_VAR:
         {
            // C_VAR is a variable that we would have initialized if we had taken another branch in an if-then-else, but since we took the branch we did, it is not initialized
            // We can initialize it now to a local var
            unsigned int slot = CODE16(PC+1);
            FR->slots[slot] = (word)&FR->slots[slot];
            PC+=3;
            continue;
         }
         default:
         {
            printf("Illegal instruction %d\n", *PC);
            assert(0 && "Illegal instruction\n");
         }
      }
   }
   return HALT;
}


void resume_execution(ExecutionCallback callback, int resume)
{
   current_yield_ptr = callback;
   //printf("Executing with %d\n", resume);
   RC rc = execute(resume);
   if (rc != YIELD)
   {
      // FIXME: Er, we cannot free these if there are choicepoints?!
      //free_clause(query->clause);
      callback(rc);
   }
}

EMSCRIPTEN_KEEPALIVE
ExecutionCallback current_yield()
{
   return current_yield_ptr;
}

EMSCRIPTEN_KEEPALIVE
void resume_yield(RC status, ExecutionCallback y)
{
   //printf("Resuming from yield: %d\n", status);
   if (status == FAIL)
   {
      //printf("Yield is fail\n");
      if (current_exception != 0)
         resume_execution(y, 0);
      else if (backtrack())
         resume_execution(y, 0);
      else
         y(FAIL);
   }
   else if (status == SUCCESS)
   {
      //printf("Yield is success\n");
      resume_execution(y, 1);
   }
   else
   {
      printf("Unexpected return code: %d\n", status);
   }
}

RC prepare_query(word goal)
{
   goal = DEREF(goal);
   if (TAGOF(goal) == VARIABLE_TAG)
   {
      instantiation_error();
      return ERROR;
   }
   Query query = compile_query(goal);
   if (query == NULL)
   {
      return ERROR;
   }

   FR = allocFrame(NULL);
   FR->functor = MAKE_FUNCTOR(MAKE_ATOM("<top>"), 1);
   FR->returnPC = PC;
   FR->clause = &exitQueryClause;
   FR->choicepoint = CP;

   NFR = allocFrame(FR);
   NFR->functor = MAKE_FUNCTOR(MAKE_ATOM("<query>"), 1);
   NFR->returnPC = FR->clause->code;
   NFR->is_local = 1;
   NFR->clause = query->clause;
   NFR->choicepoint = CP;

   FR = NFR;
   ARGP = ARGS;
   //printf("ARGP starts at %p\n", ARGP);
   for (int i = 0; i < query->variable_count; i++)
      ARGS[i] = query->variables[i];

   // Protect the variables of this frame
   SP += query->variable_count;

   PC = FR->clause->code;
   free_query(query);
   return SUCCESS;
}

RC execute_query_sync(word goal)
{
   RC rc = prepare_query(goal);
   if (rc != SUCCESS)
      return rc;
   rc = execute(0);
   assert(rc != YIELD); // Do not do this.
   return rc;
}

void execute_query(word goal, ExecutionCallback callback)
{
   RC rc = prepare_query(goal);
   if (rc != SUCCESS)
      callback(rc);
   else
      resume_execution(callback, 0);
}

void backtrack_query(ExecutionCallback callback)
{
   if (backtrack())
   {
      resume_execution(callback, 0);
   }
   else
      callback(FAIL);
}


word getException()
{
   return current_exception;
}

int clause_functor(word t, word* functor)
{
   if (TAGOF(t) == CONSTANT_TAG)
   {
      int type;
      cdata c = getConstant(t, &type);
      if (type == ATOM_TYPE)
      {
         *functor = MAKE_FUNCTOR(t, 0);
         return 1;
      }
      return type_error(callableAtom, t);
   }
   else if (FUNCTOROF(t) == clauseFunctor)
      return clause_functor(ARGOF(t, 0), functor);
   *functor = FUNCTOROF(t);
   return 1;
}

void consult_stream(Stream s)
{
   word* savedH = H;
   word* savedSP = SP;
   word clause;
   while (1)
   {
      word clause;
      if (!read_term(s, NULL, &clause))
      {
         CLEAR_EXCEPTION();
         printf("Syntax error reading file\n");
         break;
      }
      if (clause == endOfFileAtom)
         break;
      //printf("Compiling clause:\n    "); PORTRAY(clause); printf("\n");
      if (TAGOF(clause) == COMPOUND_TAG && FUNCTOROF(clause) == directiveFunctor)
      {
         word directive = ARGOF(clause, 0);
         if (TAGOF(directive) == COMPOUND_TAG && FUNCTOROF(directive) == moduleFunctor)
         {
            if (!must_be_atom(ARGOF(directive, 0)))
            {
               CLEAR_EXCEPTION();
               printf("Error in module declaration\n");
               break;
            }
            word exports = ARGOF(directive, 1);
            while (TAGOF(exports) == COMPOUND_TAG && FUNCTOROF(exports) == listFunctor)
            {
               word export = ARGOF(exports, 0);
               exports = ARGOF(exports, 1);
               if (!must_be_predicate_indicator(export))
               {
                  CLEAR_EXCEPTION();
                  printf("Illegal export\n");
                  break;
               }
               // For an export of foo/3, add a clause foo(A,B,C):- Module:foo(A,B,C).  to user
               long arity = getConstant(ARGOF(export, 1), NULL).integer_data;
               word functor = MAKE_FUNCTOR(ARGOF(export, 0), arity);
               word head = MAKE_COMPOUND(functor);
               word shim = MAKE_VCOMPOUND(clauseFunctor, head, MAKE_VCOMPOUND(crossModuleCallFunctor, ARGOF(directive, 0), head));
               add_clause(userModule, functor, shim);
            }
            if (exports != emptyListAtom)
            {
               printf("Illegal export list\n");
               break;
            }
            Module oldModule = find_module(ARGOF(directive, 0));
            if (oldModule != NULL)
            {
               destroy_module(oldModule);
               if (oldModule == currentModule)
                  currentModule = userModule;
               if (oldModule == userModule)
                  userModule = create_module(MAKE_ATOM("user"));
            }
            currentModule = create_module(ARGOF(directive, 0));
         }
         else if (TAGOF(directive) == COMPOUND_TAG && FUNCTOROF(directive) == metaPredicateFunctor)
         {
            if (!must_be_compound(ARGOF(directive, 0)))
            {
               printf("Illegal meta-predicate decalaration\n");
               break;
            }
            word functor = FUNCTOROF(ARGOF(directive, 0));
            Functor f = getConstant(functor, NULL).functor_data;
            char* meta = malloc(f->arity + 1);
            meta[f->arity] = 0;
            for (int i = 0; i < f->arity; i++)
            {
               word arg = ARGOF(ARGOF(directive, 0),i);
               int type;
               cdata c = getConstant(arg, &type);
               if (type == ATOM_TYPE)
                  meta[i] = c.atom_data->data[0];
               else if (type == INTEGER_TYPE)
                  meta[i] = c.integer_data + '0';
               else
               {
                  free(meta);
                  printf("Illegal meta-specifier\n");
                  break;
               }
               set_meta(currentModule, functor, meta);
            }
         }
         else if (TAGOF(directive) == COMPOUND_TAG && FUNCTOROF(directive) == dynamicFunctor)
         {
            if (!must_be_compound(ARGOF(directive, 0)))
            {
               printf("Illegal dynamic decalaration\n");
               break;
            }
            word functor = MAKE_FUNCTOR(ARGOF(ARGOF(directive,0),0), getConstant(ARGOF(ARGOF(directive,0),1), NULL).integer_data);
            set_dynamic(currentModule, functor);
         }
         else if (TAGOF(directive) == COMPOUND_TAG && FUNCTOROF(directive) == multiFileFunctor)
         {
         }
         else if (TAGOF(directive) == COMPOUND_TAG && FUNCTOROF(directive) == discontiguousFunctor)
         {
         }
         else if (TAGOF(directive) == COMPOUND_TAG && FUNCTOROF(directive) == initializationFunctor)
         {
         }
         else
         {
            RC rc = execute_query_sync(directive);
            if (rc == SUCCESS || rc == SUCCESS_WITH_CHOICES)
               printf("     Directive succeeded\n");
            else if (rc == FAIL)
               printf("     Directive failed\n");
            else if (rc == ERROR)
               printf("     Directive raised an exception\n");
         }
      }
      else
      {
         // Ordinary clause
         word functor;
         if (clause_functor(clause, &functor))
             add_clause(currentModule, functor, clause);
         else
         {
            printf("Error: <FIXME write the error>\n");
            CLEAR_EXCEPTION();
         }
      }
   }
   // In case this has changed, set it back after consulting any file
   currentModule = userModule;
   // Restore H and SP
   H = savedH;
   SP = savedSP;

}

int consult_file(const char* filename)
{
   Stream s = fileReadStream(filename);
   if (s != NULL)
   {
      consult_stream(s);
      s->close(s);
      freeStream(s);
      return 1;
   }
   return 0;
}

void consult_string(char* string)
{
   Stream s = stringBufferStream(string, strlen(string));
   consult_stream(s);
   freeStream(s);
}

void consult_string_of_length(char* string, int length)
{
   Stream s = stringBufferStream(string, length);
   consult_stream(s);
   freeStream(s);
}


void print_clause(Clause clause)
{
   for (int i = 0; i < clause->code_size; i++)
   {
      int flags = instruction_info[clause->code[i]].flags;
      printf("@%d: %s ", i, instruction_info[clause->code[i]].name);
      if (flags & HAS_CONST)
      {
         int index = (clause->code[i+1] << 8) | (clause->code[i+2]);
         i+=2;
         PORTRAY(clause->constants[index]); printf(" ");
      }
      if (flags & HAS_ADDRESS)
      {
         uintptr_t address = CODEPTR(&clause->code[i+1]);
         i+=sizeof(word);
         printf("%" PRIuPTR " ", address);
      }
      if (flags & HAS_SLOT)
      {
         int slot = (clause->code[i+1] << 8) | (clause->code[i+2]);
         i+=2;
         printf("%d ", slot);
      }
      printf("\n");
   }
}

void print_instruction()
{
   printf("@%p: ", PC); PORTRAY(FR->functor); printf(" (FR=%p CP=%p, SP=%p(%"PRIpd"), ARGP=%p, H=%p(%"PRIpd"), TR=%p(%"PRIpd")) %s ", FR, CP, SP, (SP-STACK), ARGP, H, (H-HEAP), TR, (TR-TRAIL), instruction_info[*PC].name);
   unsigned char* ptr = PC+1;
   if (instruction_info[*PC].flags & HAS_CONST)
   {
      int index = CODE16(ptr);
      ptr+=2;
      PORTRAY(FR->clause->constants[index]); printf(" ");
   }
   if (instruction_info[*PC].flags & HAS_ADDRESS)
   {
      uintptr_t address = CODEPTR(ptr);
      ptr+=sizeof(word);
      printf("%"PRIuPTR" ", address);
   }
   if (instruction_info[*PC].flags & HAS_SLOT)
   {
      int slot = CODE16(ptr);
      ptr+=2;
      printf("%d ", slot);
   }
   printf("\n");
}

// Note that this is not tested and may not work very well!
void make_foreign_cleanup_choicepoint(word w, void (*fn)(int, word), int arg)
{
   //printf("Saving foreign pointer to %p\n", &FR->slots[CODE16(PC+1+sizeof(word))]);
   FR->slots[CODE16(PC+1+sizeof(word))] = w;
   //printf("Allocating a foreign choicepoint at %p (top = %p, bottom = %p)\n", SP, STOP, STACK);
   Choicepoint c = (Choicepoint)SP;
   c->SP = SP;
   c->CP = CP;
   c->argc = getConstant(FR->functor, NULL).functor_data->arity;
   //printf("Saving %d args on the choicepoint\n", c->argc);
   for (int i = 0; i < c->argc; i++)
   {
//      printf("%d: ", i); PORTRAY(ARGS[i]); printf("\n");
      c->args[i] = ARGS[i];
   }
   c->H = H;
   c->type = Head;
   c->foreign_cleanup.fn = fn;
   c->foreign_cleanup.arg = arg;
   c->cleanup = NULL;
   CP = c;
   SP += sizeof(choicepoint)/sizeof(word);
   c->FR = FR;
   c->clause = FR->clause;
   c->module = currentModule;
   c->NFR = NFR;
   c->functor = FR->functor;
   c->TR = TR;
   c->PC = PC+3+sizeof(word);
   assert(FR->slots[CODE16(PC+1+sizeof(word))] == w);
}

void make_foreign_choicepoint(word w)
{
   make_foreign_cleanup_choicepoint(w, NULL, 0);
}

int head_functor(word head, Module* module, word* functor)
{
   if (TAGOF(head) == CONSTANT_TAG)
   {
      int type;
      getConstant(head, &type);
      if (type == ATOM_TYPE)
      {
         *module = FR->contextModule;
         *functor = MAKE_FUNCTOR(head, 0); // atom
         return 1;
      }
      return type_error(callableAtom, head);
   }
   else if (TAGOF(head) == COMPOUND_TAG)
   {
      if (FUNCTOROF(head) == crossModuleCallFunctor)
      {
         *module = find_module(ARGOF(head, 0));
         *functor = FUNCTOROF(ARGOF(head, 1));
      }
      else if (FUNCTOROF(head) == clauseFunctor)
      {
         return head_functor(ARGOF(head, 0), module, functor);
      }
      *functor =  FUNCTOROF(head);
      *module = FR->contextModule;
      return 1;
   }
   else if (TAGOF(head) == VARIABLE_TAG)
      return instantiation_error();
   return type_error(callableAtom, head);
}


word predicate_indicator(word term)
{
   if (TAGOF(term) == CONSTANT_TAG)
   {
      int type;
      cdata c = getConstant(term, &type);
      if (type == FUNCTOR_TYPE)
         return MAKE_VCOMPOUND(predicateIndicatorFunctor, c.functor_data->name, MAKE_INTEGER(c.functor_data->arity));
      else
         return MAKE_VCOMPOUND(predicateIndicatorFunctor, term, MAKE_INTEGER(0));
   }
   else if (TAGOF(term) == COMPOUND_TAG)
   {
      if (FUNCTOROF(term) == crossModuleCallFunctor)
      {
         Functor f = getConstant(FUNCTOROF(ARGOF(term, 1)), NULL).functor_data;
         return MAKE_VCOMPOUND(predicateIndicatorFunctor, MAKE_VCOMPOUND(crossModuleCallFunctor, ARGOF(term, 0), f->name), MAKE_INTEGER(f->arity));
      }
      Functor f = getConstant(FUNCTOROF(term), NULL).functor_data;
      return MAKE_VCOMPOUND(predicateIndicatorFunctor, f->name, MAKE_INTEGER(f->arity));
   }
   return term;
}

Module get_current_module()
{
   return FR->contextModule;
}

void halt(int i)
{
   halted = 1;
}

word get_choicepoint_depth()
{
   return MAKE_POINTER(CP);
}

long heap_usage() // Returns heap usage in bytes
{
   return ((unsigned int)H - (unsigned int)HEAP) / sizeof(void*);
}

EMSCRIPTEN_KEEPALIVE
void qqq()
{
   debugging = 0;
   //print_memory_info();
   //printf("Heap: H=%p(%"PRIpd" slots used)\n", H, H-HEAP);
   //   print_choices();
   //ctable_check();
}

EMSCRIPTEN_KEEPALIVE
void qqqy()
{
   ctable_check();
}


EMSCRIPTEN_KEEPALIVE
void* cmalloc(size_t size)
{
   return malloc(size);
}

EMSCRIPTEN_KEEPALIVE
void cfree(void* ptr)
{
   free(ptr);
}
