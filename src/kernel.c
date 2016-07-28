#include "kernel.h"
#include "constants.h"
#include "ctable.h"
#include "compiler.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>


uint8_t* PC;
int halted = 0;
Choicepoint CP = NULL;
unsigned int TR;
uintptr_t H = 0;
word trail[1024];
word HEAP[65535];
word* argStack[64];
word** argStackP = argStack;

Frame FR, NFR;
word *ARGP;
Module currentModule = NULL;
Choicepoint initialChoicepoint = NULL;


void fatal(char* string)
{
   printf("Fatal: %s\n", string);
   assert(0);
}

word MAKE_VAR()
{
   word newVar = H;
   HEAP[H] = H;
   H++;
   return newVar;
}

word MAKE_COMPOUND(word functor)
{
   word addr = H;
   HEAP[H] = functor;
   H++;
   return addr | COMPOUND_TAG;
}

word MAKE_VCOMPOUND(word functor, ...)
{
   Functor f = getConstant(functor).data.functor_data;
   va_list argp;
   va_start(argp, functor);
   word addr = H;
   HEAP[H] = functor;
   H++;
   for (int i = 0; i < f->arity; i++)
      HEAP[H++] = va_arg(argp, word);
   return addr | COMPOUND_TAG;
}

uint32_t hash(void* data, int length)
{
   return 0;
}

int atom_compare(Constant a, Constant b)
{
   Atom aa = (Atom)a;
   Atom bb = (Atom)b;
   return aa->length == bb->length && strncmp(aa->data, bb->data, aa->length) == 0;
}

int integer_compare(Constant a, Constant b)
{
   return ((Integer)a)->data == ((Integer)b)->data;
}

int functor_compare(Constant a, Constant b)
{
   return ((Functor)a)->name == ((Functor)b)->name && ((Functor)a)->arity == ((Functor)b)->arity;
}

Atom allocAtomNoCopy(char* data, int length)
{
   Atom a = malloc(sizeof(atom));
   a->data = data;
   a->length = length;
   return a;
}

Integer allocInteger(long data)
{
   Integer a = malloc(sizeof(integer));
   a->data = data;
   return a;
}

Functor allocFunctor(word name, int arity)
{
   Functor a = malloc(sizeof(functor));
   a->name = name;
   a->arity = arity;
   return a;
}


word MAKE_ATOM(char* data)
{
   Atom a = allocAtomNoCopy(data, strlen(data));
   int isNew = 0;
   word result = intern(ATOM_TYPE, (Constant)a, hash(data, strlen(data)), atom_compare, &isNew);
   if (isNew)
      a->data = strdup(data);
   else
      free(a);
   return result;
}

word MAKE_INTEGER(long data)
{
   Integer i = allocInteger(data);
   int isNew = 0;
   word result = intern(INTEGER_TYPE, (Constant)i, data, integer_compare, &isNew);
   if (!isNew)
      free(i);
   return result;
}

word MAKE_FUNCTOR(word name, int arity)
{
   Functor f = allocFunctor(name, arity);
   int isNew = 0;
   Atom n = getConstant(name).data.atom_data;
   word result = intern(FUNCTOR_TYPE, (Constant)f, hash(n->data, n->length) + arity, functor_compare, &isNew);
   if (!isNew)
      free(f);
   return result;
}

void bind(word var, word value)
{
   trail[TR++] = var;
   *((Word)var) = value;
}

word link(word value)
{
   word v = MAKE_VAR();
   bind(v, value);
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
      printf("_%lu\n", w);
   else if (TAGOF(w) == CONSTANT_TAG)
   {
      constant c = getConstant(w);
      switch(c.type)
      {
         case ATOM_TYPE:
            printf("%s", c.data.atom_data->data);
            break;
         case INTEGER_TYPE:
            printf("%ld", c.data.integer_data->data);
            break;
      }
   }
   else if (TAGOF(w) == COMPOUND_TAG)
   {
      Functor functor = getConstant(FUNCTOROF(w)).data.functor_data;
      PORTRAY(functor->name);
      for (int i = 0; i < functor->arity; i++)
      {
         PORTRAY(ARGOF(w, i));
         if (i+1 < functor->arity)
            printf(",");
      }
   }
}

int unify(word a, word b)
{
   a = DEREF(a);
   b = DEREF(b);
   if (a == b)
      return 1;
   if (TAGOF(a) == VARIABLE_TAG)
   {
      bind(a, b);
      return 1;
   }
   if (TAGOF(b) == VARIABLE_TAG)
   {
      bind(b, a);
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
      CP = CP->previous;
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

int backtrack()
{
   unsigned int oldTR = TR;
   while(1)
   {
      if (CP == NULL)
         return 0;
      if (!CP->apply())
         continue;
      unwind_trail(oldTR);
   }
}



word instantiationError()
{
   return MAKE_VCOMPOUND(errorFunctor, instantiationErrorAtom, MAKE_VAR());
}

void set_exception(word e)
{
}

void CreateChoicepoint(unsigned int address)
{
}

void CreateClauseChoicepoint()
{
}


Frame allocFrame()
{
   Frame frame = malloc(sizeof(frame));
   frame->parent = FR;
   frame->depth = (FR != NULL)?FR->depth+1:0;
   frame->slots = NULL; // !!! FIXME
   frame->reserved_slots = NULL; // FIXME
   frame->clause = NULL;
   frame->contextModule = currentModule;
   frame->returnPC = 0;
   frame->choicepoint = CP;
   return frame;
}

Module getModule(constant name)
{
   return NULL;
}

int get_code(constant functor)
{
   return 0;
}


RC execute()
{
   while (!halted)
   {
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
            if (FR->reserved_slots[CODE16(PC+1)] == (word)CP)
               CP = CP->previous;
            goto i_exit;
         case I_FOREIGN:
            FR->reserved_slots[0] = (word)0;
            // fall-through
         case I_FOREIGNRETRY:
         {
            RC rc = FAIL;
#ifdef EMSCRIPTEN
            rc = EM_ASM_INT({execute_foreign($0, $1)}, FR->reserved_slots[0], &FR->slots);
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
         case I_EXIT:
         i_exit:
         case I_EXITFACT:
            PC = FR->returnPC;
            FR = FR->parent;
            NFR = allocFrame();
            ARGP = NFR->slots;
            continue;
         case I_DEPART:
         {
            word functor = FR->clause->constants[CODE16(PC+1)];
            NFR->functor = functor;
            if (!get_code(getConstant(functor)))
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
            set_exception(*(ARGP-1));
            // fall-through
         case B_THROW_FOREIGN:
         b_throw_foreign:
         {
            // Not implemented yet
         }
         case I_SWITCH_MODULE:
         {
            word moduleName = FR->clause->constants[CODE16(PC+1)];
            currentModule = getModule(getConstant(moduleName));
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
            if (!get_code(getConstant(functor)))
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
            CreateChoicepoint(-1);
            FR->choicepoint = CP;
            FR->reserved_slots[CODE16(PC+1)] = (word)CP;
            ARGP = &FR->slots[1];
            PC+=2;
            continue;
         }
         case I_USERCALL:
         {
            word goal = DEREF(*(ARGP-1));
            if (TAGOF(goal) == VARIABLE_TAG)
            {
               set_exception(instantiationError());
               goto b_throw_foreign;
            }
            Query query = compileQuery(goal);
            NFR->functor = MAKE_FUNCTOR(MAKE_ATOM("<meta-call>"), 1);
            NFR->clause = query->clause;
            NFR->returnPC = PC+1;
            NFR->choicepoint = CP;
            ARGP = NFR->slots;
            for (int i = 0; i < query->variable_count; i++)
               NFR->slots[i] = query->variables[i];
            FR = NFR;
            NFR = allocFrame();
            PC = NFR->clause->code;
            freeQuery(query);
            continue;
         }
         case I_CUT:
            if (cut_to(FR->choicepoint) == YIELD)
               return YIELD;
            FR->choicepoint = CP;
            PC++;
            continue;
         case C_CUT:
            if (cut_to((Choicepoint)FR->reserved_slots[CODE16(PC+1)]) == YIELD)
               return YIELD;
            PC+=3;
            continue;
         case C_LCUT:
            if (cut_to((Choicepoint)FR->reserved_slots[CODE16(PC+1)] + 1) == YIELD)
               return YIELD;
            PC+=3;
            continue;
         case C_IFTHEN:
            FR->reserved_slots[CODE16(PC+1)] = (word)CP;
            PC+=3;
            continue;
         case C_IFTHENELSE:
            FR->reserved_slots[CODE16(PC+1)] = (word)CP;
            CreateChoicepoint(CODE32(PC+3));
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
            *ARGP = link(FR->slots[CODE16(PC+1)]);
            PC+=3;
            continue;
         case B_ARGVAR:
         {
            unsigned int slot = CODE16(PC+1);
            word arg = DEREF(FR->slots[slot]);
            if (TAGOF(arg) == VARIABLE_TAG)
            {
               *ARGP = MAKE_VAR();
               bind(*(ARGP++), arg);
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
            *(ARGP++) = link(FR->slots[slot]);
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
            ARGP = ARGPOF(t, 0);
            PC+=3;
            continue;
         }
         case H_FIRSTVAR:
         {
            if (mode == WRITE)
            {
               FR->slots[CODE16(PC+1)] = MAKE_VAR();
               bind(*(ARGP++),FR->slots[CODE16(PC+1)]);
            }
            else
            {
               if (TAGOF(*ARGP) == VARIABLE_TAG)
                  FR->slots[CODE16(PC+1)] = link(*(ARGP++));
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
                  ARGP = ARGPOF(arg, 0);
                  continue;
               }
            }
            else if (TAGOF(arg) == VARIABLE_TAG)
            {
               *(argStackP++) = ARGP;
               word t = MAKE_COMPOUND(functor);
               bind(arg, t);
               ARGP = ARGPOF(t, 0);
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
            if (arg == atom)
               continue;
            else if (TAGOF(arg) == VARIABLE_TAG)
               bind(arg, atom);
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
            CreateChoicepoint(CODE32(PC+1));
            PC += 5;
            continue;
         case TRY_ME_OR_NEXT_CLAUSE:
            CreateClauseChoicepoint();
            PC++;
            continue;
         case TRUST_ME:
            PC++;
            continue;
         case S_QUALIFY:
         {
            unsigned int slot = CODE16(PC+1);
            word value = DEREF(FR->slots[slot]);
            if (!(TAGOF(value) == COMPOUND_TAG && FUNCTOROF(value) == crossModuleCallFunctor))
               FR->slots[slot] = MAKE_VCOMPOUND(crossModuleCallFunctor, FR->parent->contextModule->term, value);
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
