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

uint8_t* PC;
int halted = 0;
Choicepoint CP = NULL;
unsigned int TR;

#define HEAP_SIZE 655350
#define TRAIL_SIZE 655350
#define STACK_SIZE 655350

word trail[TRAIL_SIZE];
word HEAP[HEAP_SIZE];
word* HTOP = &HEAP[HEAP_SIZE];
word* H = &HEAP[0];
word STACK[STACK_SIZE];
word* STOP = &STACK[STACK_SIZE];
word* SP = &STACK[0];
uintptr_t argStack[64];
uintptr_t* argStackP = argStack;

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

word MAKE_VAR()
{
   word newVar = (word)H;
   *H = newVar;
   assert(H < HTOP);
   H++;
   return newVar;
}

word MAKE_COMPOUND(word functor)
{
   word addr = (word)H;
   *H = functor;
   H++;
   // Make all the args vars
   int arity = getConstant(functor).data.functor_data->arity;
   assert(H + arity < HTOP);
   for (int i = 0; i < arity; i++)
   {
      MAKE_VAR();
   }
   return addr | COMPOUND_TAG;
}

word MAKE_VCOMPOUND(word functor, ...)
{
   Functor f = getConstant(functor).data.functor_data;
   va_list argp;
   va_start(argp, functor);
   word addr = (word)H;
   *H = functor;
   H++;
   assert(H + f->arity < HTOP);
   for (int i = 0; i < f->arity; i++)
   {
      word w = va_arg(argp, word);
      *(H++) = w;
   }
   return addr | COMPOUND_TAG;
}

// You MUST call MAKE_ACOMPOUND with a functor as the first arg
word MAKE_ACOMPOUND(word functor, word* values)
{
   constant c = getConstant(functor);
   assert(c.type == FUNCTOR_TYPE);
   Functor f = c.data.functor_data;
   word addr = (word)H;
   *H = functor;
   H++;
   assert(H + f->arity < HTOP);
   for (int i = 0; i < f->arity; i++)
      *(H++) = values[i];
   assert(H < HTOP);
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
   constant c = getConstant(functor);
   if (c.type == ATOM_TYPE)
      functor = MAKE_FUNCTOR(functor, list_length(list));
   Functor f = getConstant(functor).data.functor_data;
   word addr = (word)H;
   *(H++) = functor;
   assert(H + list_length(list) < HTOP);
   list_apply(list, NULL, _lcompoundarg);
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
   a->data = strdup(data);
   a->length = length;
   return a;
}


void* allocInteger(void* data, int len)
{
   Integer a = malloc(sizeof(integer));
   a->data = *((long*)data);
   return a;
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
   return intern(ATOM_TYPE, hash((unsigned char*)data, length), data, length, allocAtom, NULL);
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
   return intern_blob(type, ptr);
}


EMSCRIPTEN_KEEPALIVE
word MAKE_INTEGER(long data)
{
   return intern(INTEGER_TYPE, data, &data, sizeof(long), allocInteger, NULL);
}

EMSCRIPTEN_KEEPALIVE
word MAKE_FLOAT(double data)
{
   return intern(FLOAT_TYPE, hash64((uint64_t)data), &data, sizeof(double), allocFloat, NULL);
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
   Atom n = getConstant(name).data.atom_data;
   word w =  intern(FUNCTOR_TYPE, hash((unsigned char*)n->data, n->length) + arity, &name, arity, allocFunctor, NULL);
   return w;
}

void _bind(word var, word value)
{
   //printf("TR: %d\n", TR);
   assert(TR+1 < TRAIL_SIZE);
   trail[TR++] = var;
   *((Word)var) = value;
}

word _link(word value)
{
   word v = MAKE_VAR();
   _bind(v, value);
   return v;
}

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

void PORTRAY(word w)
{
   w = DEREF(w);
   if (TAGOF(w) == VARIABLE_TAG)
      printf("_G%" PRIuPTR, (w - (word)HEAP));
   else if (TAGOF(w) == CONSTANT_TAG)
   {
      constant c = getConstant(w);
      switch(c.type)
      {
         case ATOM_TYPE:
            printf("%.*s", c.data.atom_data->length, c.data.atom_data->data);
            break;
         case INTEGER_TYPE:
            printf("%ld", c.data.integer_data->data);
            break;
         case FUNCTOR_TYPE:
            PORTRAY(c.data.functor_data->name); printf("/%d", c.data.functor_data->arity);
            break;
         case FLOAT_TYPE:
            printf("%f", c.data.float_data->data);
            break;
         case BLOB_TYPE:
            printf("%s<%p>", c.data.blob_data->type, c.data.blob_data->ptr);
            break;
         default:
            printf("Unknown type: %d\n", c.type);
            assert(0);

      }
   }
   else if (TAGOF(w) == COMPOUND_TAG)
   {
      //printf("Pointer to a compound %08lx. This points to a functor at %08lx\n", w, w&~3);
      Functor functor = getConstant(FUNCTOROF(w)).data.functor_data;
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

int unify(word a, word b)
{
   a = DEREF(a);
   b = DEREF(b);
   if (a == b)
      return SUCCESS;
   if (TAGOF(a) == VARIABLE_TAG)
   {
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
      int arity = getConstant(FUNCTOROF(a)).data.functor_data->arity;
      for (int i = 0; i < arity; i++)
         if (!unify(ARGOF(a, i), ARGOF(b, i)))
            return 0;
      return SUCCESS;
   }
   return 0;
}


void unwind_trail(unsigned int from)
{
   for (int i = TR; i < from; i++)
   {
      *((Word)trail[i]) = trail[i];
      //printf("Unbound variable %p\n", trail[i]);
   }
}


// After executing, cut_to(X), we ensure that X is the current choicepoint.
int cut_to(Choicepoint point)
{
   while (CP > point)
   {
      if (CP == NULL) // Fatal, I guess?
         fatal("Cut to non-existent choicepoint");
      Choicepoint c = CP;
      CP = CP->CP;
      //printf("Cleanup: %p\n", c->cleanup);
      if (c->cleanup != NULL)
      {
         if (c->cleanup->foreign != NULL)
         {
            // FIXME: call a foreign goal
         }
         else
         {
            if (unify(c->cleanup->catcher, cutAtom))
            {
               // FIXME: call a prolog goal
            }
         }
      }
   }
   return 1;
}

int apply_choicepoint(Choicepoint c)
{
   //printf("Applying choicepoint at %p\n", c);
   PC = c->PC;
   //printf("PC is now %p\n", PC);
   H = c->H;
   FR = c->FR;
   NFR = c->NFR;
   SP = c->SP;
   CP = c->CP;
   TR = c->TR;
   FR->clause = c->clause;
   currentModule = c->module;
   if (c->type == Head)
   {
      //printf("Applying head choice. ARGP is set to %p\n", FR->slots);
      mode = READ;
      ARGP = FR->slots;
   }
   else
   {
      ARGP = NFR->slots;
   }
   return (PC != 0);
}

int backtrack()
{
   //printf("Backtracking: %p\n", CP);
   unsigned int oldTR = TR;
   while(1)
   {
      if (CP == NULL)
      {
         return 0;
      }
      if (!apply_choicepoint(CP))
         continue;
      unwind_trail(oldTR);
      return 1;
   }
}

int backtrack_to(Choicepoint c)
{
   // Undoes to the given choicepoint, undoing everything else in between
   while (CP != c)
   {
      if (CP == NULL)
         return 0;
      unsigned int oldTR = TR;
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
      Functor functor = getConstant(FUNCTOROF(t)).data.functor_data;
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
      Functor f = getConstant(FUNCTOROF(t)).data.functor_data;
      for (int j = 0; j < f->arity; j++)
         i+=count_compounds(ARGOF(t, j));
      return i + f->arity;
   }
   return 0;
}

word make_local_(word t, List* variables, word* heap, int* ptr, int extra)
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
         Functor f = getConstant(FUNCTOROF(t)).data.functor_data;
         (*ptr) += f->arity;
         for (int i = 0; i < f->arity; i++)
         {
            heap[argp++] = make_local_(ARGOF(t, i), variables, heap, ptr, extra);
         }
         return result;
      }
      case VARIABLE_TAG:
      {
         int i = list_index(variables, t);
         heap[i+extra] = (word)&heap[i+extra];
         return (word)&heap[i+extra];
      }
   }
   assert(0);
}

word copy_local_with_extra_space(word t, word** local, int extra)
{
   List variables;
   init_list(&variables);
   int i = count_compounds(t);
   find_variables(t, &variables);
   i += list_length(&variables);
   i += extra;
   *local = malloc(sizeof(word) * i);
   int ptr = extra+list_length(&variables);
   word w = make_local_(t, &variables, *local, &ptr, extra);
   free_list(&variables);
   return w;
}

word copy_local(word t, word** local)
{
   return copy_local_with_extra_space(t, local, 0);
}

void CreateChoicepoint(unsigned char* address, Clause clause, int type)
{
   //printf("Creating a choicepoint at %p with frame %p and continuation address %p\n", SP, FR, address);
   Choicepoint c = (Choicepoint)SP;
   c->SP = SP;
   c->CP = CP;
   c->H = H;
   CP = c;
   SP += sizeof(choicepoint)/sizeof(word);
   //printf("Choicepoint extends until %p\n", SP);
   c->type = type;
   c->FR = FR;
   c->clause = clause;
   c->module = currentModule;
   c->cleanup = NULL;
   c->NFR = NFR;
   c->functor = FR->functor;
   c->TR = TR;
   c->PC = address;
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
int prepare_frame(word functor, Module optionalContext, Frame frame)
{
   Module module = (optionalContext != NULL)?optionalContext:currentModule;
   Predicate p = lookup_predicate(module, functor);
   if (p == NULL && currentModule != userModule)
   {
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
      Functor f = getConstant(functor).data.functor_data;
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
         return 0;
   }
   frame->is_local = 0;
   frame->parent = FR;
   if (FR != NULL)
      frame->depth = FR->depth+1;
   else
      frame->depth = 0;
   frame->choicepoint = CP;
   return 1;
}

Frame allocFrame()
{
   //printf("Making a frame at %p\n", SP);
   Frame f = (Frame)SP;
   SP += sizeof(frame)/sizeof(word);
   //printf("frame extends until %p\n", SP);
   f->parent = FR;
   if (FR != NULL)
      f->depth = FR->depth+1;
   else
      f->depth = 0;
   f->clause = NULL;
   f->contextModule = currentModule;
   f->returnPC = 0;
   f->choicepoint = CP;
   return f;
}

void initialize_kernel()
{
   userModule = create_module(MAKE_ATOM("user"));
   currentModule = userModule;
   current_input = nullStream();
   current_output = consoleOuputStream();
   initialize_foreign();
   consult_file("src/builtin.pl");
}


RC execute()
{
   //printf("SP starts at %p\n", SP);
   //printf("sizeof(frame) = %d\n", sizeof(frame));
   while (!halted)
   {
      //print_choices();
      //print_instruction();
      switch(*PC)
      {
         case I_FAIL:
            if (backtrack())
               continue;
            return FAIL;
         case I_ENTER:
            // Note that at I_ENTER, ARGP is already pointing the last argument we matched in the head. We must check there is enough space for the variables in the frame
            // There are two cases: The stack contains a choicepoint above ARGP, or it doesnt. If it does, then SP will have been adjusted already and we must leave it alone
            if ((uintptr_t)CP < (uintptr_t)FR)
            {
               // The top of the stack needs to be protected before we can allocate the frame
               //printf("Protecting frame by moving SP from %p to %p (%p + %d + %d)\n", SP, ((word*)FR) + FR->clause->slot_count + sizeof(frame) / sizeof(word), ((word*)FR), FR->clause->slot_count, sizeof(frame) / sizeof(word));
               SP = ((word*)FR) + FR->clause->slot_count + sizeof(frame) / sizeof(word);
            }
            NFR = (Frame)SP;
            //printf("Allocated a frame for the body to populate: %p (with parent %p). However, SP is being left at %p\n", NFR, FR, SP);
            ARGP = NFR->slots;
            //printf("FR->returnPC = %p\n", FR->returnPC);
            PC++;
            continue;
         case I_EXIT_QUERY:
            if (CP == initialChoicepoint)
               return SUCCESS;
            return SUCCESS_WITH_CHOICES;
         case I_EXITCATCH:
            if (FR->slots[CODE16(PC+1)] == (word)CP)
               CP = CP->CP;
            goto i_exit;
         case I_FOREIGN:
         {
            RC rc = FAIL;
            Functor f = getConstant(FR->functor).data.functor_data;
            //printf("argP is at %p\n", ARGP);
            //PORTRAY(*ARGP); printf("\n");
            //printf("Calling %p with %p (%p)\n", (int (*)())((word)(CODEPTR(PC+1))), ARGP, FR->slots);
            switch(f->arity)
            {
               case 0: rc = ((int (*)())((word)(CODEPTR(PC+1))))(); break;
               case 1: rc = ((int (*)(word))((word)(CODEPTR(PC+1))))(DEREF(*ARGP)); break;
               case 2: rc = ((int (*)(word,word))((word)(CODEPTR(PC+1))))(DEREF(DEREF(*ARGP)), DEREF(*(ARGP+1))); break;
               case 3: rc = ((int (*)(word,word,word))((word)(CODEPTR(PC+1))))(DEREF(DEREF(*ARGP)), DEREF(*(ARGP+1)), DEREF(*(ARGP+2))); break;
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
            if (current_exception != 0)
               goto b_throw_foreign;
            if (rc == FAIL)
            {
               //printf("Foreign Fail\n");
               if (backtrack())
                  continue;
               return FAIL;
            }
            else if (rc == SUCCESS)
            {
               //printf("Foreign Success!\n");
               goto i_exit;
            }
            else if (rc == YIELD)
            { // Not implemented yet
            }
            else if (rc == ERROR)
               goto b_throw_foreign;
         }
         case I_FOREIGN_NONDET:
         case I_FOREIGN_JS:
            //printf("Setting slot %d to NULL\n", CODE16(PC+1+sizeof(word)));
            FR->slots[CODE16(PC+1+sizeof(word))] = (word)0;
            // We have to move PC forward since the retry step will rewind it
            PC += (3+sizeof(word));
            // fall-through
         case I_FOREIGN_JS_RETRY:
         case I_FOREIGN_RETRY:
         {
            RC rc = FAIL;
            Functor f = getConstant(FR->functor).data.functor_data;
            if (*PC == I_FOREIGN_RETRY || *PC == I_FOREIGN_NONDET)
            {
               // Go back to the actual FOREIGN_NONDET or FOREIGN_JS call
               PC -= (3+sizeof(word));
               unsigned char* foreign_ptr = PC+1+sizeof(word);
               //printf("Calling %p with %p (%p, %p) SP=%p\n", (int (*)())((word)(CODEPTR(PC+1))), ARGP, FR, FR->slots, SP);
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
            else
            {
#ifdef EMSCRIPTEN
               PC -= (3+sizeof(word));
               unsigned char* foreign_ptr = PC+1+sizeof(word);
               rc = EM_ASM_INT({_foreign_call($0, $1, $2, $3)}, FR->slots[CODE16(foreign_ptr)], CODEPTR(PC+1), f->arity, ARGP);
#else
               rc = SET_EXCEPTION(existence_error(procedureAtom, MAKE_VCOMPOUND(predicateIndicatorFunctor, f->name, MAKE_INTEGER(f->arity))));
#endif
            }
            if (current_exception != 0)
               goto b_throw_foreign;
            if (rc == FAIL)
            {
               //printf("Foreign Fail\n");
               if (backtrack())
                  continue;
               return FAIL;
            }
            else if (rc == SUCCESS)
            {
               //printf("Foreign Success!\n");
               goto i_exit;
            }
            else if (rc == YIELD)
            { // Not implemented yet
            }
            else if (rc == ERROR)
            {
               goto b_throw_foreign;
            }
         }
         i_exit:
         //printf("   Jumped to I_EXIT\n");
         case I_EXIT:
         case I_EXIT_FACT:
         {
            //printf("Exiting from frame %p, to parent %p\n", FR, FR->parent);
            if ((uintptr_t)CP < (uintptr_t)FR->parent)
            {
               //printf("No choicepoints. Current frame is %p, CP is %p\n", FR, CP);
               // There are no choicepoints newer than this frame. We can just set SP back to the current frame pointer
               SP = (word*)FR;
            }
            else
            {
               //printf("Choicepoint at %p\n", CP);
               // There is a choicepoint newer than this frame. This limits how far we can wind SP back
               SP = (word*)CP + sizeof(choicepoint)/sizeof(word);
            }
            //printf("   Wound SP back to %p\n", SP);
            PC = FR->returnPC;
            if (FR->is_local && (word)FR > (word)CP)
            {
               // Free the clause if it was generated specifically for this frame and we cannot possibly backtrack into it
               free_clause(FR->clause);
            }
            FR = FR->parent;
            NFR = (Frame)SP;
            //printf("FR is now %p\n", FR);
            //NFR = SP;
            //printf("Set NFR to %p, but leaving SP at %p\n", NFR, SP);
            ARGP = NFR->slots;
            continue;
         }
         case I_DEPART:
         {
            word functor = FR->clause->constants[CODE16(PC+1)];
            if (!prepare_frame(functor, NULL, NFR))
               goto b_throw_foreign;
            NFR->returnPC = FR->returnPC;
            //printf("Setting parent of frame %p to be %p\n", NFR, FR->parent);
            NFR->parent = FR->parent;
            ARGP = NFR->slots;
            if (FR->is_local && (word)FR > (word)CP)
            {
               // Free the clause if it was generated specifically for this frame and we cannot possibly backtrack into it
               free_clause(FR->clause);
            }
            FR = NFR;
            if ((uintptr_t)CP < (uintptr_t)FR)
            {
               SP = ARGP + FR->clause->slot_count;
               //printf("Fencing in arguments by moving SP to %p (%d slots)\n", SP, FR->clause->slot_count);
            }
            FR->choicepoint = CP;
            // Deallocate the frame entirely by moving SP back down, provided there are no choicepoints in the way
            //if (CP < FR)
            //   SP = FR;
            PC = FR->clause->code;
            continue;
         }
         case B_CLEANUP_CHOICEPOINT:
         {
            // Not implemented yet
            PC++;
            continue;
         }
         case B_THROW:
            SET_EXCEPTION(*(ARGP-1));
            // fall-through
         case B_THROW_FOREIGN:
         b_throw_foreign:
         {
            //printf("in b_throw_foreign\n");
            if (current_exception == 0)
               assert(0 && "throw but not exception?");
            List backtrace;
            init_list(&backtrace);
            //printf("Looking for handler for "); PORTRAY(current_exception); printf("\n");
            while(FR != NULL)
            {
               //printf("Checking for catch/3 in %p\n", FR); PORTRAY(FR->functor); printf("\n");
               list_append(&backtrace, FR->functor);
               if (FR->functor == catchFunctor)
               {
                  //printf("Found a catch/3 in %p\n", FR);
                  //printf("SP is now %p\n", SP);
                  backtrack_to(FR->choicepoint);

                  // And then undo the fake choicepoint. Note that this resets H destroying all terms created since the catch was called
                  apply_choicepoint(CP);
                  //printf("Unwound to catch/3. CP is now %p\n", CP);
                  //printf("Current exception is now "); PORTRAY(current_exception); printf("\n");
                  //printf("Unifier is %08lx at %p\n", FR->slots[1], &FR->slots[1]);
                  //PORTRAY(FR->slots[0]); printf("\n");
                  //PORTRAY(FR->slots[1]); printf("\n");
                  if (unify(copy_term(current_exception), FR->slots[1]))
                  {
                     // Success! Exception is successfully handled.
                     if (TAGOF(current_exception) == COMPOUND_TAG && FUNCTOROF(current_exception) == errorFunctor && (TAGOF(ARGOF(current_exception, 1)) == VARIABLE_TAG))
                     {
                        unify(ARGOF(DEREF(FR->slots[1]), 1), make_backtrace(&backtrace));
                     }
                     free_list(&backtrace);
                     CLEAR_EXCEPTION();

                     // Now we just have to do i_usercall after adjusting the registers to point to the handler
                     // Things get a bit weird here because we are breaking the usual logic flow by ripping the VM out of whatever state it was in and starting
                     // it off in a totally different place. We have to reset argP, argI and PC then pretend the next instruction was i_usercall
                     ARGP = FR->slots + 3;
                     PC = &FR->clause->code[12];
                     FR->functor = caughtFunctor;
                     // Adjust for USERCALL so it sets up the right return address - we want to pretend this is where we are current executing
                     PC = FR->returnPC-1;
                     FR = FR->parent;
                     goto i_usercall;
                  }
                  else
                  {
                     //  printf("... but the handler does not unify\n");
                  }
               }
               FR = FR->parent;
            }
            free_list(&backtrace);
            return ERROR;
         }
         case I_SWITCH_MODULE:
         {
            word moduleName = FR->clause->constants[CODE16(PC+1)];
            currentModule = find_module(moduleName);
            FR->contextModule = currentModule;
            PC+=3;
            continue;
         }
         case I_EXITMODULE:
            PC++;
            continue;
         case I_CALL:
         {
            word functor = FR->clause->constants[CODE16(PC+1)];
            //printf("Frame at %p has functor ", NFR); PORTRAY(functor); printf("\n");
            assert((word*)NFR < STOP);
            if (!prepare_frame(functor, NULL, NFR))
               goto b_throw_foreign;
            NFR->returnPC = PC+3;
            ARGP = NFR->slots;
            FR = NFR;
            if ((uintptr_t)CP < (uintptr_t)FR)
            {
               SP = ARGP + FR->clause->slot_count;
               //printf("Fencing in arguments by moving SP to %p (%d slots)\n", SP, FR->clause->slot_count);
            }
            //printf("Parent is %p\n", FR->parent);
            FR->choicepoint = CP;
            PC = FR->clause->code;
            continue;
         }
         case I_CATCH:
         {
            CreateChoicepoint(0, FR->clause, Head);
            FR->choicepoint = CP;
            //printf("FR is %p, parent is %p\n", FR, FR->parent);
            FR->slots[CODE16(PC+1)] = (word)CP;
            ARGP = &FR->slots[1];
            NFR = (Frame)SP;
            //printf("NFR set to %p\n", NFR);
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
            //PORTRAY(goal); printf(" compiles to\n");
            //print_clause(query->clause);
            NFR->parent = FR;
            //printf("Functor of usercall frame %p is set to <meta-call>/1\n", NFR);
            NFR->functor = MAKE_FUNCTOR(MAKE_ATOM("<meta-call>"), 1);
            NFR->clause = query->clause;
            NFR->is_local = 1;
            NFR->returnPC = PC+1;
            NFR->choicepoint = CP;
            ARGP = NFR->slots;
            for (int i = 0; i < query->variable_count; i++)
            {
               //printf("Populating meta-call arg %d at location %p\n", i, &NFR->slots[i]);
               NFR->slots[i] = query->variables[i];
            }
            FR = NFR;
            if ((uintptr_t)CP < (uintptr_t)FR)
            {
               SP = ARGP + FR->clause->slot_count;
               //printf("Fencing in meta-call arguments by moving SP to %p (%d slots)\n", SP, FR->clause->slot_count);
            }
            //printf(" usercall frame is now set to %p\n", FR);
            PC = FR->clause->code;
            free_query(query);
            continue;
         }
         case I_CUT:
            //printf("Cutting to %p\n", FR->choicepoint);
            if (cut_to(FR->choicepoint) == YIELD)
               return YIELD;
            //printf("Done\n");
            FR->choicepoint = CP;
            PC++;
            continue;
         case C_CUT:
         {

            //print_choices();
            //printf("Cut to %p from %p\n", FR->slots[CODE16(PC+1)], CP);
            if (cut_to(((Choicepoint)FR->slots[CODE16(PC+1)])) == YIELD)
               return YIELD;
            PC+=3;
            continue;
         }
         case C_LCUT:
         {
            Choicepoint C = CP;
            //printf("CP is %p\n", CP);
            //printf("We are looking for the choicepoint after %p\n", ((Choicepoint)FR->slots[CODE16(PC+1)]));
            while (C > ((Choicepoint)FR->slots[CODE16(PC+1)]))
            {
               assert(C != NULL); // The choicepoint we are looking for is gone
               if (C->CP == ((Choicepoint)FR->slots[CODE16(PC+1)]))
               {
                  //printf("Found it\n");
                  if (cut_to(C) == YIELD)
                     return YIELD;
                  PC+=3;
                  break;
               }
            }
            continue;
         }
         case C_IF_THEN:
            FR->slots[CODE16(PC+1)] = (word)CP;
            //printf("Saved the current choicepoint in %p: %p\n", &FR->slots[CODE16(PC+1)], CP);
            PC+=3;
            continue;
         case C_IF_THEN_ELSE:
         {
            //printf("Saving the current choicepoint of CP %p in %p\n", CP, &FR->slots[CODE16(PC+1+sizeof(word))] );
            FR->slots[CODE16(PC+1+sizeof(word))] = (word)CP;
            unsigned char* address = PC+CODEPTR(PC+1);
            //printf("Creating a choicepoint at %p with a continuation of %p \n", SP, address);
            //printf("Opcode at %p -> %d\n", address, *address);
            //printf("FR->clause; %p\n",FR->clause);
            CreateChoicepoint(address, FR->clause, Body);
            NFR = (Frame)(SP + sizeof(frame)/sizeof(word));
            ARGP = NFR->slots;
            PC+=3 + sizeof(word);
            continue;
         }
         case I_UNIFY:
         {
            word t1 = *(ARGP-1);
            word t2 = *(ARGP-2);
            //PORTRAY(t1); printf(" vs "); PORTRAY(t2); printf("\n");
            ARGP-=2;
            if (unify(t1, t2))
            {
               PC++;
               //printf("Unified\n");
               continue;
            }
            //printf("Failed to unify\n");
            if (backtrack())
               continue;
            return FAIL;
         }
         case B_FIRSTVAR:
            //printf("Making var in slot %d: At %p\n", CODE16(PC+1), &FR->slots[CODE16(PC+1)]);
            FR->slots[CODE16(PC+1)] = MAKE_VAR();
            //printf("Writing var to %p\n", ARGP);
            *(ARGP++) = _link(FR->slots[CODE16(PC+1)]);
            PC+=3;
            continue;
         case B_ARGVAR:
         {
            unsigned int slot = CODE16(PC+1);
            word arg = DEREF(FR->slots[slot]);
            if (TAGOF(arg) == VARIABLE_TAG)
            {
               *ARGP = MAKE_VAR();
               _bind(*(ARGP++), arg);
            }
            else
               *(ARGP++) = arg;
            PC+=3;
            continue;
         }
         case B_VAR:
         {
            unsigned int slot = CODE16(PC+1);
            //printf("Copying var from slot %d at %p\n", slot, &FR->slots[slot]);
            //printf("Writing this at %p\n", ARGP);
            if (FR->slots[slot] == 0)
            {
               printf("... suspicious! There is nothing in the arg slot at %p\n", &FR->slots[slot]);
               printf("Writing to %p\n", &FR->slots[slot]);
               assert(0);
               FR->slots[slot] = MAKE_VAR();
            }
            //printf("Writing linked variable to %p\n", ARGP);
            //printf("NFR is %p\n", NFR);
            //printf("Variable should deref to %08lx\n which is:", FR->slots[slot]);
            //PORTRAY(FR->slots[slot]); printf("\n");
            word w = _link(FR->slots[slot]);
            //printf("But actual value is %08lx\n", w);
            *(ARGP++) = w;
            PC+=3;
            continue;
         }
         case B_POP:
            ARGP = (word*)*(--argStackP);
            //printf("ARGP is now to %p\n", ARGP);
            PC++;
            continue;
         case B_ATOM:
            //printf("Writing atom to %p\n", ARGP);
            *(ARGP++) = FR->clause->constants[CODE16(PC+1)];
            PC+=3;
            continue;
         case B_VOID:
            *(ARGP++) = MAKE_VAR();
            PC++;
            continue;
         case B_FUNCTOR:
         {
            word t = MAKE_COMPOUND(FR->clause->constants[CODE16(PC+1)]);
            //printf("NFR->parent: %p\n", NFR->parent);
            *(ARGP++) = t;
            *(argStackP++) = (uintptr_t)ARGP;
            ARGP = ARGPOF(t);
            PC+=3;
            continue;
         }
         case H_FIRSTVAR:
         {
            if (mode == WRITE)
            {
               //printf("Write mode. ArgP is %p \n", ARGP);
               //PORTRAY(*ARGP); printf("\n");
               FR->slots[CODE16(PC+1)] = MAKE_VAR();
               _bind(*(ARGP++),FR->slots[CODE16(PC+1)]);
               //printf("Written\n");
            }
            else
            {
               if (TAGOF(*ARGP) == VARIABLE_TAG)
                  FR->slots[CODE16(PC+1)] = _link(*(ARGP++));
               else
                  FR->slots[CODE16(PC+1)] = *(ARGP++);
            }
            PC+=3;
            continue;
         }
         case H_FUNCTOR:
         {
            word functor = FR->clause->constants[CODE16(PC+1)];
            word arg = DEREF(*(ARGP++));
            PC+=3;
            if (TAGOF(arg) == COMPOUND_TAG)
            {
               if (FUNCTOROF(arg) == functor)
               {
                  *(argStackP++) = (uintptr_t)ARGP | mode;
                  ARGP = ARGPOF(arg);
                  continue;
               }
            }
            else if (TAGOF(arg) == VARIABLE_TAG)
            {
               *(argStackP++) = (uintptr_t)ARGP | mode;
               word t = MAKE_COMPOUND(functor);
               _bind(arg, t);
               ARGP = ARGPOF(t);

               mode = WRITE;
               continue;
            }
            if (backtrack())
               continue;
            return FAIL;
         }
         case H_POP:
         {
            uintptr_t t = *--argStackP;
            mode = t & 1;
            ARGP = (word*)(t & ~1);
            PC++;
            continue;
         }
         case H_ATOM:
         {
            //printf("Arg is at %p\n", ARGP);
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
            else if (backtrack())
               continue;
            return FAIL;
         }
         case H_VOID:
            //printf("Frame is %p, parent is %p, SP is %p ignoring arg at %p, with value %08lx\n which is ", FR, FR->parent, SP, ARGP, *ARGP);
            //PORTRAY(*ARGP); printf("\n");
            ARGP++;
            PC++;
            continue;
         case H_VAR:
            if (!unify(DEREF(*(ARGP++)), FR->slots[CODE16(PC+1)]))
            {
               if (backtrack())
                  continue;
               return FAIL;
            }
            PC+=3;
            continue;
         case C_JUMP:
            PC += CODEPTR(PC+1);
            continue;
         case C_OR:
            CreateChoicepoint(PC+CODEPTR(PC+1), FR->clause, Body);
            NFR = (Frame)(SP + sizeof(frame)/sizeof(word));
            ARGP = NFR->slots;
            PC += 1 + sizeof(word);
            continue;
         case TRY_ME_OR_NEXT_CLAUSE:
         {
//            SP += FR->clause->slot_count;
            //printf("Creating a choicepoint at %p\n", SP);
            //printf("ReturnPC was %p\n", FR->returnPC);
            // We must ensure that this clause has enough space
            if (SP < ((word*)FR) + FR->clause->slot_count + sizeof(frame) / sizeof(word))
            {
               // FIXME: We could also shrink the space here if needed. In fact, is there any reason we cannot just ALWAYS set SP to this?
               //printf("Not enough space for this clause: We have %p but require %p bytes. Enlarging....\n", (char*)SP - (char*)FR, sizeof(word)*FR->clause->slot_count + sizeof(frame));
               SP = ((word*)FR) + FR->clause->slot_count + sizeof(frame) / sizeof(word);
               //printf("Bumping up SP to ensure this frame has enough space: Moving to %p\n", SP);
            }
            CreateChoicepoint(FR->clause->next->code, FR->clause->next, Head);
            //printf("ReturnPC is now %p\n", FR->returnPC);
            //printf("Done\n");
            PC++;
            continue;
         }
         case TRUST_ME:
            PC++;
            continue;
         case S_QUALIFY:
         {
            unsigned int slot = CODE16(PC+1);
            word value = DEREF(FR->slots[slot]);
            if (!(TAGOF(value) == COMPOUND_TAG && FUNCTOROF(value) == crossModuleCallFunctor))
               FR->slots[slot] = MAKE_VCOMPOUND(crossModuleCallFunctor, FR->parent->contextModule->name, value);
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


RC execute_query(word goal)
{
   goal = DEREF(goal);
   if (TAGOF(goal) == VARIABLE_TAG)
   {
      instantiation_error();
      return ERROR;
   }
   Query query = compile_query(goal);
   if (query == NULL)
      return ERROR;
   FR = allocFrame();
   FR->functor = MAKE_FUNCTOR(MAKE_ATOM("<top>"), 1);
   FR->returnPC = NULL;
   FR->clause = &exitQueryClause;
   FR->choicepoint = NULL;

   NFR = allocFrame();
   NFR->functor = MAKE_FUNCTOR(MAKE_ATOM("<query>"), 1);
   NFR->returnPC = FR->clause->code;
   NFR->clause = query->clause;
   NFR->choicepoint = CP;

   FR = NFR;
   ARGP = FR->slots;
   //printf("ARGP starts at %p\n", ARGP);
   for (int i = 0; i < query->variable_count; i++)
      FR->slots[i] = query->variables[i];

   // Protect the variables of this frame
   SP += query->variable_count;

//   NFR = allocFrame();
   PC = FR->clause->code;
   free_query(query);
   return execute();
}

RC backtrack_query()
{
   if (backtrack())
      return execute();
   return FAIL;
}


word getException()
{
   return current_exception;
}

int clause_functor(word t, word* functor)
{
   if (TAGOF(t) == CONSTANT_TAG)
   {
      constant c = getConstant(t);
      if (c.type == ATOM_TYPE)
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
               long arity = getConstant(ARGOF(export, 1)).data.integer_data->data;
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
            Functor f = getConstant(functor).data.functor_data;
            char* meta = malloc(f->arity + 1);
            for (int i = 0; i < f->arity; i++)
            {
               word arg = ARGOF(ARGOF(directive, 0),i);
               constant c = getConstant(arg);
               if (c.type == ATOM_TYPE)
                  meta[i] = c.data.atom_data->data[0];
               else if (c.type == INTEGER_TYPE)
                  meta[i] = c.data.integer_data->data + '0';
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
            word functor = MAKE_FUNCTOR(ARGOF(ARGOF(directive,0),0), getConstant(ARGOF(ARGOF(directive,0),1)).data.integer_data->data);
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
            RC rc = execute_query(directive);
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

}

void consult_file(char* filename)
{
   Stream s = fileReadStream(filename);
   consult_stream(s);
   s->close(s);
   freeStream(s);
}

void consult_string(char* string)
{
   Stream s = stringBufferStream(string, strlen(string));
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
   printf("@%p: ", PC); PORTRAY(FR->functor); printf(" (FR=%p, CP=%p, SP=%p, ARGP=%p, H=%p, TR=%d) %s ", FR, CP, SP, ARGP, H, TR, instruction_info[*PC].name);
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

void make_foreign_choicepoint(word w)
{
   //printf("Saving foreign pointer to %p\n", &FR->slots[CODE16(PC+1+sizeof(word))]);
   FR->slots[CODE16(PC+1+sizeof(word))] = w;
   //printf("Allocating a foreign choicepoint at %p (top = %p, bottom = %p)\n", SP, STOP, STACK);
   Choicepoint c = (Choicepoint)SP;
   c->SP = SP;
   c->CP = CP;
   c->H = H;
   c->type = Body;
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

int head_functor(word head, Module* module, word* functor)
{
   if (TAGOF(head) == CONSTANT_TAG)
   {
      if (getConstant(head).type == ATOM_TYPE)
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
      constant c = getConstant(term);
      if (c.type == FUNCTOR_TYPE)
         return MAKE_VCOMPOUND(predicateIndicatorFunctor, c.data.functor_data->name, MAKE_INTEGER(c.data.functor_data->arity));
      else
         return MAKE_VCOMPOUND(predicateIndicatorFunctor, term, MAKE_INTEGER(0));
   }
   else if (TAGOF(term) == COMPOUND_TAG)
   {
      if (FUNCTOROF(term) == crossModuleCallFunctor)
      {
         Functor f = getConstant(FUNCTOROF(ARGOF(term, 1))).data.functor_data;
         return MAKE_VCOMPOUND(predicateIndicatorFunctor, MAKE_VCOMPOUND(crossModuleCallFunctor, ARGOF(term, 0), f->name), MAKE_INTEGER(f->arity));
      }
      Functor f = getConstant(FUNCTOROF(term)).data.functor_data;
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
