#ifdef EMSCRIPTEN
#include <emscripten/emscripten.h>
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
#include <stdio.h>
#include <stdarg.h>
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
uintptr_t H = 0;
uintptr_t SP = 0;

word trail[1024];
word HEAP[65535];
word STACK[65535];
word* argStack[64];
word** argStackP = argStack;

Frame FR, NFR;
word *ARGP;
Module currentModule = NULL;
Module userModule = NULL;
Choicepoint initialChoicepoint = NULL;
word current_exception = 0;

void fatal(char* string)
{
   printf("Fatal: %s\n", string);
   assert(0);
}

word MAKE_VAR()
{
   word newVar = (word)&HEAP[H];
   HEAP[H] = newVar;
   H++;
   return newVar;
}

word MAKE_COMPOUND(word functor)
{
   word addr = (word)&HEAP[H];
   HEAP[H] = functor;
   H++;
   return addr | COMPOUND_TAG;
}

word MAKE_VCOMPOUND(word functor, ...)
{
   Functor f = getConstant(functor).data.functor_data;
   va_list argp;
   va_start(argp, functor);
   word addr = (word)&HEAP[H];
   HEAP[H] = functor;
   H++;
   for (int i = 0; i < f->arity; i++)
      HEAP[H++] = va_arg(argp, word);
   return addr | COMPOUND_TAG;
}

// You MUST call MAKE_ACOMPOUND with a functor as the first arg
word MAKE_ACOMPOUND(word functor, word* values)
{
   constant c = getConstant(functor);
   assert(c.type == FUNCTOR_TYPE);
   Functor f = c.data.functor_data;
   word addr = (word)&HEAP[H];
   HEAP[H] = functor;
   H++;
   for (int i = 0; i < f->arity; i++)
      HEAP[H++] = values[i];
   return addr | COMPOUND_TAG;
}

void _lcompoundarg(word w, void* ignored)
{
   HEAP[H++] = w;
}

// You can call MAKE_LCOMPOUND with either an atom or a functor as the first arg.
// If you call it with an atom, it will make a functor with arity list->size for you
word MAKE_LCOMPOUND(word functor, List* list)
{
   constant c = getConstant(functor);
   if (c.type == ATOM_TYPE)
      functor = MAKE_FUNCTOR(functor, list_length(list));
   Functor f = getConstant(functor).data.functor_data;
   word addr = (word)&HEAP[H];
   HEAP[H++] = functor;
   list_apply(list, NULL, _lcompoundarg);
   return addr | COMPOUND_TAG;
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


void* allocFunctor(void* data, int len)
{
   Functor f = malloc(sizeof(functor));
   f->name = *((word*)data);
   f->arity = len;
   return f;
}

EMSCRIPTEN_KEEPALIVE
word MAKE_NATOM(char* data, size_t length)
{
   return intern(ATOM_TYPE, hash((unsigned char*)data, length), data, length, allocAtom, NULL);
}

EMSCRIPTEN_KEEPALIVE
word MAKE_ATOM(char* data)
{
   return MAKE_NATOM(data, strlen(data));
}

EMSCRIPTEN_KEEPALIVE
word MAKE_INTEGER(long data)
{
   return intern(INTEGER_TYPE, data, &data, sizeof(long), allocInteger, NULL);
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
      printf("_G%d", (w - (word)HEAP));
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

      }
   }
   else if (TAGOF(w) == COMPOUND_TAG)
   {
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
   else
      printf("Bad tag\n");
}

void SET_EXCEPTION(word w)
{
   current_exception = w;
}

int unify(word a, word b)
{
   a = DEREF(a);
   b = DEREF(b);
   if (a == b)
      return 1;
   if (TAGOF(a) == VARIABLE_TAG)
   {
      _bind(a, b);
      return 1;
   }
   if (TAGOF(b) == VARIABLE_TAG)
   {
      _bind(b, a);
      return 1;
   }
   if ((TAGOF(a) == COMPOUND_TAG) && (TAGOF(b) == COMPOUND_TAG) && (FUNCTOROF(a) == FUNCTOROF(b)))
   {
      int arity = getConstant(FUNCTOROF(a)).data.functor_data->arity;
      for (int i = 0; i < arity; i++)
         if (!unify(ARGOF(a, i), ARGOF(b, i)))
            return 0;
      return 1;
   }
   return 0;
}


void unwind_trail(unsigned int from)
{
   for (int i = TR; i < from; i++)
      *((Word)trail[i]) = trail[i];
}


int cut_to(Choicepoint point)
{
   while (CP != point)
   {
      if (CP == NULL) // Fatal, I guess?
         fatal("Cut to non-existent choicepoint");
      Choicepoint c = CP;
      CP = CP->CP;
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
   PC = c->PC;
   FR = c->FR;
   NFR = c->NFR;
   SP = c->SP;
   CP = c->CP;
   TR = c->TR;
   FR->clause = c->clause;
   currentModule = c->module;
   ARGP = FR->slots;
   return (PC != 0);
}

int backtrack()
{
   //printf("Backtracking: %p\n", CP);
   unsigned int oldTR = TR;
   while(1)
   {
      if (CP == NULL)
         return 0;
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

void CreateChoicepoint(unsigned char* address, Clause clause)
{
   Choicepoint c = (Choicepoint)&STACK[SP];
   c->SP = SP;
   c->CP = CP;
   CP = c;
   SP += sizeof(choicepoint);
   c->FR = FR;
   c->clause = clause;
   c->module = currentModule;
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

int load_frame_code(word functor, Module optionalContext, Frame frame)
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
         existence_error(procedureAtom, MAKE_VCOMPOUND(predicateIndicatorFunctor, f->name, MAKE_INTEGER(f->arity)));
         return 0;
      }
      else if (get_prolog_flag("unknown") == failAtom)
      {
         frame->clause = &failClause;
         return 1;
      }
      else if (get_prolog_flag("unknown") == warningAtom)
      {
         printf("No such predicate "); PORTRAY(f->name); printf("/%d\n", f->arity);
         frame->clause = &failClause;
         return 1;
      }
   }
   frame->clause = get_predicate_code(p);
   //print_clause(frame->clause);
   return 1;
}

Frame allocFrame()
{
   Frame f = (Frame)&STACK[SP];
   SP += sizeof(frame);
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
}

RC execute()
{
   while (!halted)
   {
      //printf("@%p: %s\n", PC, instruction_info[*PC].name);
      switch(*PC)
      {
         case I_FAIL:
            if (backtrack())
               continue;
            return FAIL;
         case I_ENTER:
            NFR = allocFrame();
            ARGP = NFR->slots;
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
         case I_FOREIGN_JS:
            FR->slots[CODE16(PC+5)] = (word)0;
            // fall-through
         case I_FOREIGN_RETRY:
         {
            RC rc = FAIL;
            Functor f = getConstant(FR->functor).data.functor_data;
#ifdef EMSCRIPTEN
            rc = EM_ASM_INT({_foreign_call($0, $1, $2, $3)}, FR->slots[CODE16(PC+5)], CODE32(PC+1), f->arity, ARGP);
#endif
            if (rc == FAIL)
            {
               if (backtrack())
                  continue;
               return FAIL;
            }
            else if (rc == SUCCESS)
               goto i_exit;
            else if (rc == YIELD)
            { // Not implemented yet
            }
            else if (rc == ERROR)
               goto b_throw_foreign;
         }
         i_exit:
         case I_EXIT:
         case I_EXIT_FACT:
         {
            PC = FR->returnPC;
            FR = FR->parent;
            NFR = allocFrame();
            ARGP = NFR->slots;
            continue;
         }
         case I_DEPART:
         {
            word functor = FR->clause->constants[CODE16(PC+1)];
            NFR->functor = functor;
            if (!load_frame_code(functor, NULL, NFR))
               goto b_throw_foreign;
            NFR->returnPC = FR->returnPC;
            NFR->parent = FR->parent;
            ARGP = NFR->slots;
            FR = NFR;
            FR->choicepoint = CP;
            NFR = allocFrame();
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
            if (current_exception == 0)
               assert(0 && "throw but not exception?");
            word backtrace = 0;
            if (FUNCTOROF(current_exception) == errorFunctor && (TAGOF(ARGOF(current_exception, 1)) == VARIABLE_TAG))
               backtrace = ARGOF(current_exception, 1);

            while(FR != NULL)
            {
               if (backtrace != 0)
               {
                  if (FR->parent == NULL)
                  {
                     Functor f = getConstant(FR->functor).data.functor_data;
                     unify(backtrace, MAKE_VCOMPOUND(predicateIndicatorFunctor, f->name, MAKE_INTEGER(f->arity)));
                     backtrace = 0;
                  }
                  else
                  {
                     word bnext = MAKE_VAR();
                     Functor f = getConstant(FR->functor).data.functor_data;
                     unify(backtrace, MAKE_VCOMPOUND(listFunctor, bnext, MAKE_VCOMPOUND(predicateIndicatorFunctor, f->name, MAKE_INTEGER(f->arity))));
                     backtrace = bnext;
                  }
               }
               if (FR->functor == catchFunctor)
               {
                  backtrack_to(FR->choicepoint);
                  if (unify(copy_term(current_exception), FR->slots[1]))
                  {
                     // Success! Exception is successfully handled. Now we just have to do i_usercall after adjusting the registers to point to the handler
                     // Things get a bit weird here because we are breaking the usual logic flow by ripping the VM out of whatever state it was in and starting
                     // it off in a totally different place. We have to reset argP, argI and PC then pretend the next instruction was i_usercall
                     ARGP = FR->slots + 3;
                     PC = &FR->clause->code[12];
                     FR->functor = caughtFunctor;
                     goto i_usercall;
                  }
               }
               FR = FR->parent;
            }
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
            NFR->functor = functor;
            if (!load_frame_code(functor, NULL, NFR))
               goto b_throw_foreign;
            NFR->returnPC = PC+3;
            ARGP = NFR->slots;
            FR = NFR;
            FR->choicepoint = CP;
            NFR = allocFrame();
            PC = FR->clause->code;
            continue;
         }
         case I_CATCH:
         {
            CreateChoicepoint(0, FR->clause);
            FR->choicepoint = CP;
            FR->slots[CODE16(PC+1)] = (word)CP;
            ARGP = &FR->slots[1];
            PC+=2;
            continue;
         }
         case I_USERCALL:
         i_usercall:
         {
            word goal = DEREF(*(ARGP-1));
            if (TAGOF(goal) == VARIABLE_TAG)
            {
               instantiation_error();
               goto b_throw_foreign;
            }
            Query query = compile_query(goal);
            NFR->functor = MAKE_FUNCTOR(MAKE_ATOM("<meta-call>"), 1);
            NFR->clause = query->clause;
            NFR->returnPC = PC+1;
            NFR->choicepoint = CP;
            ARGP = NFR->slots;
            for (int i = 0; i < query->variable_count; i++)
               NFR->slots[i] = query->variables[i];
            FR = NFR;
            NFR = allocFrame();
            PC = FR->clause->code;
            free_query(query);
            continue;
         }
         case I_CUT:
            if (cut_to(FR->choicepoint) == YIELD)
               return YIELD;
            FR->choicepoint = CP;
            PC++;
            continue;
         case C_CUT:
            if (cut_to((Choicepoint)FR->slots[CODE16(PC+1)]) == YIELD)
               return YIELD;
            PC+=3;
            continue;
         case C_LCUT:
            if (cut_to((Choicepoint)FR->slots[CODE16(PC+1)] + 1) == YIELD)
               return YIELD;
            PC+=3;
            continue;
         case C_IF_THEN:
            FR->slots[CODE16(PC+1)] = (word)CP;
            PC+=3;
            continue;
         case C_IF_THEN_ELSE:
            FR->slots[CODE16(PC+1)] = (word)CP;
            CreateChoicepoint(PC+CODE32(PC+3), FR->clause);
            PC+=7;
            continue;
         case I_UNIFY:
         {
            word t1 = *(ARGP-1);
            word t2 = *(ARGP-2);
            ARGP-=2;
            if (unify(t1, t2))
            {
               PC++;
               continue;
            }
            if (backtrack())
               continue;
            return FAIL;
         }
         case B_FIRSTVAR:
            FR->slots[CODE16(PC+1)] = MAKE_VAR();
            *ARGP = _link(FR->slots[CODE16(PC+1)]);
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
            if (FR->slots[slot] == 0)
               FR->slots[slot] = MAKE_VAR();
            *(ARGP++) = _link(FR->slots[slot]);
            PC+=3;
            continue;
         }
         case B_POP:
            ARGP = *(--argStackP);
            PC++;
            continue;
         case B_ATOM:
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
            *(ARGP++) = t;
            *(argStackP++) = ARGP;
            ARGP = ARGPOF(t);
            PC+=3;
            continue;
         }
         case H_FIRSTVAR:
         {
            if (mode == WRITE)
            {
               FR->slots[CODE16(PC+1)] = MAKE_VAR();
               _bind(*(ARGP++),FR->slots[CODE16(PC+1)]);
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
                  *(argStackP++) = ARGP;
                  ARGP = ARGPOF(arg);
                  continue;
               }
            }
            else if (TAGOF(arg) == VARIABLE_TAG)
            {
               *(argStackP++) = ARGP;
               word t = MAKE_COMPOUND(functor);
               _bind(arg, t);
               ARGP = ARGPOF(t);
               mode = WRITE;
            }
            else if (backtrack())
               continue;
            return FAIL;
         }
         case H_POP:
            ARGP = *--argStackP;
            PC++;
            continue;
         case H_ATOM:
         {
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
            PC += CODE32(PC+1);
            continue;
         case C_OR:
            CreateChoicepoint(PC+CODE32(PC+1), FR->clause);
            PC += 5;
            continue;
         case TRY_ME_OR_NEXT_CLAUSE:
         {
            CreateChoicepoint(FR->clause->next->code, FR->clause->next);
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
            printf("Illegal instruction\n");
            return ERROR;
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
   for (int i = 0; i < query->variable_count; i++)
      FR->slots[i] = query->variables[i];
   NFR = allocFrame();
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

word clause_functor(word t)
{
   if (TAGOF(t) == CONSTANT_TAG)
   {
      constant c = getConstant(t);
      if (c.type == ATOM_TYPE)
         return MAKE_FUNCTOR(t, 0);
   }
   else if (FUNCTOROF(t) == clauseFunctor)
      return clause_functor(ARGOF(t, 0));
   return FUNCTOROF(t);
}

void consult_string(char* string)
{
   Stream s = stringBufferStream(string, strlen(string));
   word clause;
   while ((clause = read_term(s, NULL)) != endOfFileAtom)
   {
      if (TAGOF(clause) == COMPOUND_TAG && FUNCTOROF(clause) == directiveFunctor)
      {
         assert(0);
      }
      else
      {
         // Ordinary clause
         word functor = clause_functor(clause);
         add_clause(currentModule, functor, clause);
      }
   }
   freeStream(s);
   // In case this has changed, set it back after consulting any file
   currentModule = userModule;
}

void print_clause(Clause clause)
{
   for (int i = 0; i < clause->code_size; i++)
   {
      printf("@%d: %s ", i, instruction_info[clause->code[i]].name);
      if (instruction_info[clause->code[i]].flags & HAS_CONST)
      {
         int index = (clause->code[i+1] << 8) | (clause->code[i+2]);
         i+=2;
         PORTRAY(clause->constants[index]); printf(" ");
      }
      if (instruction_info[clause->code[i]].flags & HAS_ADDRESS)
      {
         long address = (clause->code[i+1] << 24) | (clause->code[i+2] << 16) | (clause->code[i+3] << 8) | (clause->code[i+4]);
         i+=4;
         printf("%lu ", address);
      }
      if (instruction_info[clause->code[i]].flags & HAS_SLOT)
      {
         int slot = (clause->code[i+1] << 8) | (clause->code[i+2]);
         i+=2;
         printf("%d ", slot);
      }
      printf("\n");
   }
}
