#include "global.h"
#include "compiler.h"
#include "list.h"
#include "kernel.h"
#include "whashmap.h"
#include "list.h"
#include "ctable.h"
#include "errors.h"
#include "constants.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <inttypes.h>

typedef struct
{
   word variable;
   int fresh;
   int slot;
   int guaranteed_safe;
} var_info_t;

struct instruction_t
{
   struct instruction_t* next;
   unsigned char opcode;
   word constant;
   int slot;
   uintptr_t address;
   int size;
   int constant_count;
};

typedef struct instruction_t instruction_t;

typedef struct
{
   instruction_t* head;
   instruction_t* tail;
   int count;
}instruction_list_t;

typedef struct
{
   instruction_list_t* left;
   instruction_list_t* right;
   wmap_t right_map;
   int* left_size;
   int* right_size;
} cvar_context_t;

void init_instruction_list(instruction_list_t* list)
{
   list->head = NULL;
   list->tail = NULL;
   list->count = 0;
}

void deinit_instruction_list(instruction_list_t* list)
{
   instruction_t* cell = list->head;
   while(cell != NULL)
   {
      instruction_t* next = cell->next;
      free(cell);
      cell = next;
   }
}


void instruction_list_apply(instruction_list_t* list, void* data, void(*fn)(void*, instruction_t*))
{
   for (instruction_t* cell = list->head; cell;)
   {
      instruction_t* next = cell->next;
      fn(data, cell); // This is allowed to change cell->next!
      cell = next;
   }
}

void _print_instruction(void* ignored, instruction_t* i)
{
   printf("%s\n", instruction_info[i->opcode].name);
}

int push_instruction(instruction_list_t* list, instruction_t* i)
{
   list->count++;
   i->next = NULL;
   if (list->tail == NULL)
   {
      list->tail = i;
      list->head = i;
   }
   else
   {
      list->tail->next = i;
      list->tail = i;
   }
   return i->size;
}

void _append_instruction(void* list, instruction_t* i)
{
   push_instruction((instruction_list_t*)list, i);
}


instruction_t* INSTRUCTION(unsigned char opcode)
{
   instruction_t* i = malloc(sizeof(instruction_t));
   i->opcode = opcode;
   i->constant = (word)-1;
   i->slot = -1;
   i->address = -1;
   i->size = 1;
   i->constant_count = 0;
   return i;
}

instruction_t* INSTRUCTION_SLOT(unsigned char opcode, int slot)
{
   instruction_t* i = malloc(sizeof(instruction_t));
   i->opcode = opcode;
   i->constant = (word)-1;
   i->slot = slot;
   i->address = -1;
   i->size = 3;
   i->constant_count = 0;
   return i;
}

instruction_t* INSTRUCTION_FUNC_SLOT(unsigned char opcode, int(*func)(), int slot)
{
   instruction_t* i = malloc(sizeof(instruction_t));
   i->opcode = opcode;
   i->constant = (word)-1;
   i->slot = slot;
   i->address = (uintptr_t)func;
   i->size = 3+sizeof(word);
   i->constant_count = 0;
   return i;
}


instruction_t* INSTRUCTION_SLOT_ADDRESS(unsigned char opcode, int slot, uintptr_t address)
{
   instruction_t* i = malloc(sizeof(instruction_t));
   i->opcode = opcode;
   i->constant = (word)-1;
   i->slot = slot;
   i->address = address;
   i->size = 3+sizeof(word);
   i->constant_count = 0;
   return i;
}

instruction_t* INSTRUCTION_ADDRESS(unsigned char opcode, uintptr_t address)
{
   instruction_t* i = malloc(sizeof(instruction_t));
   i->opcode = opcode;
   i->constant = (word)-1;
   i->slot = -1;
   i->address = address;
   i->size = 1+sizeof(word);
   i->constant_count = 0;
   return i;
}



instruction_t* INSTRUCTION_CONST(unsigned char opcode, word constant)
{
   instruction_t* i = malloc(sizeof(instruction_t));
   i->opcode = opcode;
   i->constant = constant;
   i->slot = -1;
   i->address = -1;
   i->size = 3;
   i->constant_count = 1;
   return i;
}

int instruction_count(instruction_list_t* list)
{
   return list->count;
}

var_info_t* VarInfo(word variable, int fresh, int slot, int guaranteed_safe)
{
   var_info_t* v = malloc(sizeof(var_info_t));
   v->variable = variable;
   v->fresh = fresh;
   v->slot = slot;
   v->guaranteed_safe = guaranteed_safe;
   return v;
}

int free_varinfo(any_t ignored, word key, any_t value)
{
   free(value);
   return MAP_OK;
}


any_t _copy_varinfo(any_t varinfo)
{
   var_info_t* v = (var_info_t*)varinfo;
   return VarInfo(v->variable, v->fresh, v->slot, v->guaranteed_safe);
}

int _make_cvars(any_t cx, word variable, any_t vi)
{
   var_info_t* leftvar = (var_info_t*)vi;
   cvar_context_t* context = (cvar_context_t*)cx;
   var_info_t* rightvar;
   assert(whashmap_get(context->right_map, variable, (any_t*)&rightvar) == MAP_OK);
   if (leftvar->fresh && !rightvar->fresh)
   {
      *(context->left_size) += push_instruction(context->left, INSTRUCTION_SLOT(C_VAR, leftvar->slot));
      leftvar->fresh = 0; // Var is no longer fresh
   }
   else if (!leftvar->fresh && rightvar->fresh)
   {
      *(context->right_size) += push_instruction(context->right, INSTRUCTION_SLOT(C_VAR, rightvar->slot));
   }
   // Additionally, if the left var was deemed safe, but the right wasn't then the var is no longer safe
   // Consider code like
   // foo:-
   //    ( bar(C)
   //    ; true
   //    ),
   //    bar(C).
   // If we take the first choice, then C is safe and we would omit B_VAR for the last subgoal. However, if we backtrack then
   // this leaves us with a C_VAR that is a local variable. We cannot recompile the predicate at that point, and we do not know (yet)
   // whether C is used again after the disjunction, so the only safe choice is to mark it as unsafe again.

   if (leftvar->guaranteed_safe && !rightvar->guaranteed_safe)
   {
      leftvar->guaranteed_safe = 0;
   }
   return MAP_OK;
}

int compile_argument(word arg, wmap_t variables, instruction_list_t* instructions, int embedded)
{
   if (TAGOF(arg) == VARIABLE_TAG)
   {
      var_info_t* varinfo;
      assert(whashmap_get(variables, arg, (any_t)&varinfo) == MAP_OK);
      if (varinfo->fresh)
      {
         varinfo->fresh = 0;
         varinfo->guaranteed_safe = 1; // Var is now safe (at least, in this branch)
         push_instruction(instructions, INSTRUCTION_SLOT(H_FIRSTVAR, varinfo->slot));
         return 0;
      }
      else
      {
         varinfo->guaranteed_safe = 1; // Var is now safe
         push_instruction(instructions, INSTRUCTION_SLOT(H_VAR, varinfo->slot));
         return 0;
      }
   }
   else if (TAGOF(arg) == CONSTANT_TAG)
   {
      push_instruction(instructions, INSTRUCTION_CONST(H_ATOM, arg));
      return 0;
   }
   else if (TAGOF(arg) == COMPOUND_TAG)
   {
      push_instruction(instructions, INSTRUCTION_CONST(H_FUNCTOR, FUNCTOROF(arg)));
      Functor f = getConstant(FUNCTOROF(arg), NULL).functor_data;
      for (int i = 0; i < f->arity; i++)
         compile_argument(ARGOF(arg, i), variables, instructions, 1);
      push_instruction(instructions, INSTRUCTION(H_POP));
      return 0;
   }
   return 0;
}

void compile_head(word term, wmap_t variables, instruction_list_t* instructions)
{
   int optimize_ref = instruction_count(instructions);
   if (TAGOF(term) == COMPOUND_TAG)
   {
      Functor f = getConstant(FUNCTOROF(term), NULL).functor_data;
      for (int i = 0; i < f->arity; i++)
      {
         if (!compile_argument(ARGOF(term, i), variables, instructions, 0))
            optimize_ref = instruction_count(instructions);
      }
   }
   if (optimize_ref < instruction_count(instructions) && 0)
   {
      // This is a very simple optimization: If we ONLY have h_void instructions before i_enter then we can entirely omit them
      // FIXME: For some reason this causes things to go horribly wrong. See the test suite
      //instructions.splice(optimizeRef, instructions.length);
   }
}

int compile_term_creation(word term, wmap_t variables, instruction_list_t* instructions, int depth, word parent, int isFinalArg)
{
   int needs_bpop = 0;
   int size = 0;
   do
   {
      if (TAGOF(term) == VARIABLE_TAG)
      {
         var_info_t* varinfo;
         assert(whashmap_get(variables, term, (any_t)&varinfo) == MAP_OK);
         if (varinfo->fresh)
         {
            if (!varinfo->guaranteed_safe)
            {
               //assert(0);
               if (depth > 0)
                  size += push_instruction(instructions, INSTRUCTION_SLOT(B_ARGFIRSTVAR, varinfo->slot)); // I think this is the same as what would otherwise be ARGFIRSTUNSAFEVAR
               else
                  size += push_instruction(instructions, INSTRUCTION_SLOT(B_FIRSTUNSAFEVAR, varinfo->slot));
               varinfo->guaranteed_safe = 1; // Var is now safe
            }
            else
            {
               if (depth > 0)
                  size += push_instruction(instructions, INSTRUCTION_SLOT(B_ARGFIRSTVAR, varinfo->slot));
               else
                  size += push_instruction(instructions, INSTRUCTION_SLOT(B_FIRSTVAR, varinfo->slot));
            }
            varinfo->fresh = 0;
         }
         else if (depth > 0)
         {
            size += push_instruction(instructions, INSTRUCTION_SLOT(B_ARGVAR, varinfo->slot));
         }
         else
         {
            if (!varinfo->guaranteed_safe)
            {
               size += push_instruction(instructions, INSTRUCTION_SLOT(B_UNSAFEVAR, varinfo->slot));
               varinfo->guaranteed_safe = 1; // Var is now safe
            }
            else
               size += push_instruction(instructions, INSTRUCTION_SLOT(B_VAR, varinfo->slot));
         }
         break;
      }
      else if (TAGOF(term) == COMPOUND_TAG)
      {
         if (isFinalArg)
            size += push_instruction(instructions, INSTRUCTION_CONST(B_RFUNCTOR, FUNCTOROF(term)));
         else
         {
            size += push_instruction(instructions, INSTRUCTION_CONST(B_FUNCTOR, FUNCTOROF(term)));
            needs_bpop++;
         }
         isFinalArg = 0;
         Functor f = getConstant(FUNCTOROF(term), NULL).functor_data;
         for (int i = 0; i < f->arity-1; i++)
            size += compile_term_creation(ARGOF(term, i), variables, instructions, depth+1, term, 0);
         parent = term;
         depth++;
         isFinalArg = 1;
         term = ARGOF(term, f->arity-1);
         continue;
      }
      else if (TAGOF(term) == CONSTANT_TAG)
      {
         size += push_instruction(instructions, INSTRUCTION_CONST(B_ATOM, term));
         break;
      }
      else
      {
         assert(0 && "Illegal tag");
      }
   } while(1);
   while(needs_bpop--)
      size += push_instruction(instructions, INSTRUCTION(B_POP));
   return size;
}

int compile_body(word term, wmap_t variables, instruction_list_t* instructions, int is_tail, int* next_reserved, int local_cut, int* sizep)
{
   int rc = 1;
   term = DEREF(term);
   int size = 0;
   if (TAGOF(term) == VARIABLE_TAG)
   {
      size += compile_term_creation(term, variables, instructions, 0, 0, 0); // FIXME: Is the parent of a usercall really 0?
      size += push_instruction(instructions, INSTRUCTION(I_USERCALL));
      if (is_tail)
         size += push_instruction(instructions, INSTRUCTION(I_EXIT));
   }
   else if (term == cutAtom)
   {
      if (local_cut != -1)
         size += push_instruction(instructions, INSTRUCTION_SLOT(C_LCUT, local_cut));
      else
         size += push_instruction(instructions, INSTRUCTION(I_CUT));
      if (is_tail)
         size += push_instruction(instructions, INSTRUCTION(I_EXIT));
   }
   else if (term == trueAtom)
   {
      if (is_tail)
         size += push_instruction(instructions, INSTRUCTION(I_EXIT));
   }
   else if (term == catchAtom)
   {
      int slot = 4;
      size += push_instruction(instructions, INSTRUCTION_SLOT(I_CATCH, slot));
      size += push_instruction(instructions, INSTRUCTION_SLOT(I_EXITCATCH, slot));
   }
   else if (term == failAtom)
   {
      size += push_instruction(instructions, INSTRUCTION(I_FAIL));
   }
   else if (TAGOF(term) == CONSTANT_TAG)
   {
      int type;
      cdata c = getConstant(term, &type);
      if (type == ATOM_TYPE)
      {
         size += push_instruction(instructions, INSTRUCTION_CONST(is_tail?I_DEPART:I_CALL, MAKE_FUNCTOR(term, 0)));
      }
      else
      {
         return 0;
      }
   }
   else if (TAGOF(term) == COMPOUND_TAG)
   {
      if (FUNCTOROF(term) == conjunctionFunctor)
      {
         rc &= compile_body(ARGOF(term,0), variables, instructions, 0, next_reserved, local_cut, &size);
         rc &= compile_body(ARGOF(term,1), variables, instructions, is_tail, next_reserved, local_cut, &size);
      }
      else  if (FUNCTOROF(term) == disjunctionFunctor)
      {
         if (TAGOF(ARGOF(term, 0)) == COMPOUND_TAG && FUNCTOROF(ARGOF(term,0)) == localCutFunctor)
         {
            // if-then-else
            int s1 = 0;
            int s2 = 0;
            int cut_point = (*next_reserved)++;
            instruction_t* if_then_else = INSTRUCTION_SLOT_ADDRESS(C_IF_THEN_ELSE, cut_point, -1);
            s1 += push_instruction(instructions, if_then_else);
            wmap_t varcopy = whashmap_copy(variables, _copy_varinfo);
            // If
            rc &= compile_body(ARGOF(ARGOF(term,0),0), variables, instructions, 0, next_reserved, cut_point, &s1);
            // (Cut)
            s1 += push_instruction(instructions, INSTRUCTION_SLOT(C_CUT, cut_point));
            // Then
            rc &= compile_body(ARGOF(ARGOF(term,0),1), variables, instructions, is_tail, next_reserved, local_cut, &s1);
            // (and now jump out before the Else. Note that this may not be used if is_tail is true, since the Then may be an I_DEPART.
            //  however, the Then may be something like true/0, in which case we must still provide a way out)
            instruction_t* jump = INSTRUCTION_ADDRESS(C_JUMP, -1);
            if_then_else->address = s1; // s1 is the size of C_IF_THEN_ELSE, if, C_CUT, then
            size += s1;
            s1 = 0;
            instruction_list_t else_instructions;
            init_instruction_list(&else_instructions);
            rc &= compile_body(ARGOF(term, 1), varcopy, &else_instructions, is_tail, next_reserved, local_cut, &s1);
            size += s1;
            // Compute C_VAR instructions
            cvar_context_t context;
            int s3 = 0;
            int s4 = 0;
            context.right_map = varcopy;
            context.left = instructions;
            context.right = &else_instructions;
            context.left_size = &s3;
            context.right_size = &s4;
            whashmap_iterate(variables, _make_cvars, &context);
            jump->address = s1 + s4;
            if_then_else->address +=  s3; // s1 is the size any C_VAR appearing in the if-side
            s1 += s3 + s4;
            s2 = push_instruction(instructions, jump);
            size += s2 + s3+ s4;
            if_then_else->address += s2;      // s2 is the size of the jump
            jump->address += s2;
            instruction_list_apply(&else_instructions, instructions, _append_instruction);
            if (is_tail)
               size += push_instruction(instructions, INSTRUCTION(I_EXIT));
            whashmap_iterate(varcopy, free_varinfo, NULL);
            whashmap_free(varcopy);
         }
         else
         {
            // ordinary disjunction
            int s1 = 0;
            int s2 = 0;
            instruction_t* or = INSTRUCTION_ADDRESS(C_OR, -1);
            s1 += push_instruction(instructions, or);
            wmap_t varcopy = whashmap_copy(variables, _copy_varinfo);
            rc &= compile_body(ARGOF(term, 0), variables, instructions, is_tail, next_reserved, local_cut, &s1);
            instruction_t* jump;
            if (is_tail)
               jump = INSTRUCTION(I_EXIT);
            else
               jump = INSTRUCTION_ADDRESS(C_JUMP, -1);
            size += s1;
            or->address = s1;
            s1 = 0;
            instruction_list_t else_instructions;
            init_instruction_list(&else_instructions);
            rc &= compile_body(ARGOF(term, 1), varcopy, &else_instructions, is_tail, next_reserved, local_cut, &s1);
            if (!is_tail)
            {
               // Compute C_VAR instructions
               cvar_context_t context;
               int s3 = 0;
               int s4 = 0;
               context.right_map = varcopy;
               context.left = instructions;
               context.right = &else_instructions;
               context.left_size = &s3;
               context.right_size = &s4;
               whashmap_iterate(variables, _make_cvars, &context);
               jump->address = s1 + s4;
               or->address += s3;
               s1 += s3 + s4;
            }
            s2 = push_instruction(instructions, jump);
            if (!is_tail)
            {
               jump->address += s2;
            }
            or->address += s2;
            size += s1+s2;
            instruction_list_apply(&else_instructions, instructions, _append_instruction);
            // This is not required - it would delete the instructions. appending them to the other list ensures they will be freed
            // deinit_instruction_list(&else_instructions);
            whashmap_iterate(varcopy, free_varinfo, NULL);
            whashmap_free(varcopy);
         }
      }
      else if (FUNCTOROF(term) == notFunctor)
      {
          // \+A is the same as (A -> fail ; true)
         // It is compiled to a much simpler representation though:
         //    c_if_then_else N
         //    <code for A>
         //    c_cut
         //    i_fail
         // N: (rest of the clause)
         int cut_point = (*next_reserved)++;
         int s1 = 0;
         instruction_t* if_then_else = INSTRUCTION_SLOT_ADDRESS(C_IF_THEN_ELSE, cut_point, -1);
         s1 += push_instruction(instructions, if_then_else);
         // If
         rc &= compile_body(ARGOF(term, 0), variables, instructions, 0, next_reserved, cut_point, &s1);
         // (cut)
         s1 += push_instruction(instructions, INSTRUCTION_SLOT(C_CUT, cut_point));
         // Then
         s1 += push_instruction(instructions, INSTRUCTION(I_FAIL));
         // Else
         if_then_else->address = s1;
         size += s1;
         if (is_tail)
            size += push_instruction(instructions, INSTRUCTION(I_EXIT));
         if (!rc)
         {
            CLEAR_EXCEPTION();
            return type_error(callableAtom, ARGOF(term, 0));
         }
      }
      else if (FUNCTOROF(term) == throwFunctor)
      {
         size += compile_term_creation(ARGOF(term, 0), variables, instructions, 0, 0, 0);
         size += push_instruction(instructions, INSTRUCTION(B_THROW));
      }
      else if (FUNCTOROF(term) == crossModuleCallFunctor)
      {
         size += push_instruction(instructions, INSTRUCTION_CONST(I_SWITCH_MODULE, ARGOF(term, 0)));
         rc &= compile_body(ARGOF(term, 1), variables, instructions, 0, next_reserved, local_cut, &size);
         size += push_instruction(instructions, INSTRUCTION(I_EXITMODULE));
         if (is_tail)
            size += push_instruction(instructions, INSTRUCTION(I_EXIT));
      }
      else if (FUNCTOROF(term) == cleanupChoicepointFunctor)
      {
         size += compile_term_creation(ARGOF(term, 0), variables, instructions, 0, 0, 0);
         size += compile_term_creation(ARGOF(term, 1), variables, instructions, 0, 0, 0);
         size += push_instruction(instructions, INSTRUCTION(B_CLEANUP_CHOICEPOINT));
      }
      else if (FUNCTOROF(term) == localCutFunctor)
      {
         int cut_point = (*next_reserved)++;
         size += push_instruction(instructions, INSTRUCTION_SLOT(C_IF_THEN, cut_point));
         rc &= compile_body(ARGOF(term, 0), variables, instructions, 0, next_reserved, cut_point, &size);
         size += push_instruction(instructions, INSTRUCTION_SLOT(C_CUT, cut_point));
         rc &= compile_body(ARGOF(term, 1), variables, instructions, 0, next_reserved, local_cut, &size);
         if (is_tail)
            size += push_instruction(instructions, INSTRUCTION(I_EXIT));
      }
      else if (FUNCTOROF(term) == notUnifiableFunctor)
      {
         rc &= compile_body(MAKE_VCOMPOUND(notFunctor, MAKE_VCOMPOUND(unifyFunctor, ARGOF(term, 0), ARGOF(term, 1))), variables, instructions, is_tail, next_reserved, local_cut, &size);
      }
      else if (FUNCTOROF(term) == unifyFunctor)
      {
         size += compile_term_creation(ARGOF(term, 0), variables, instructions, 0, 0, 0);
         size += compile_term_creation(ARGOF(term, 1), variables, instructions, 0, 0, 0);
         size += push_instruction(instructions, INSTRUCTION(I_UNIFY));
         if (is_tail)
            size += push_instruction(instructions, INSTRUCTION(I_EXIT));
      }
      else
      {
         Functor f = getConstant(FUNCTOROF(term), NULL).functor_data;
         for (int i = 0; i < f->arity; i++)
            size += compile_term_creation(ARGOF(term, i), variables, instructions, 0, term, 0);
         size += push_instruction(instructions, INSTRUCTION_CONST(is_tail?I_DEPART:I_CALL, FUNCTOROF(term)));
      }
   }
   else
   {
      rc = 0;
   }
   if (sizep != NULL)
      (*sizep) += size;
   return rc;
}



void find_variables(word term, List* list)
{
   term = DEREF(term);
   if (TAGOF(term) == VARIABLE_TAG && !list_contains(list, term))
      list_append(list, term);
   else if (TAGOF(term) == COMPOUND_TAG)
   {
      Functor f = getConstant(FUNCTOROF(term), NULL).functor_data;
      for (int i = 0; i < f->arity; i++)
         find_variables(ARGOF(term, i), list);
   }
}


int analyze_variables(word term, int is_head, int depth, wmap_t map, int* next_slot, word parent)
{
   do
   {
      int rc = 0;
      if (TAGOF(term) == VARIABLE_TAG)
      {
         var_info_t* varinfo;
         if (whashmap_get(map, term, (any_t)&varinfo) == MAP_MISSING)
         {
            //printf("Allocating slot %d to variable ", *next_slot); PORTRAY(term); printf(" last_term is %08lx): \n", is_head?0:term); PORTRAY(parent); printf("\n");
            varinfo = VarInfo(term, 1, (*next_slot)++, is_head);
            whashmap_put(map, term, varinfo);
            rc++;
         }
      }
      else if (TAGOF(term) == COMPOUND_TAG)
      {
         Functor f = getConstant(FUNCTOROF(term), NULL).functor_data;
         for (int i = 0; i < f->arity-1; i++)
            rc += analyze_variables(ARGOF(term, i), is_head, depth+1, map, next_slot, term);
         term = ARGOF(term, f->arity-1);
         depth++;
         continue;
      }
      return rc;
   } while(1);
}

int get_reserved_slots(word t)
{
   if (TAGOF(t) == COMPOUND_TAG)
   {
      if (FUNCTOROF(t) == conjunctionFunctor)
         return get_reserved_slots(ARGOF(t, 0)) + get_reserved_slots(ARGOF(t, 1));
      if (FUNCTOROF(t) == disjunctionFunctor)
         return get_reserved_slots(ARGOF(t, 0)) + get_reserved_slots(ARGOF(t, 1));
      if (FUNCTOROF(t) == localCutFunctor)
         return get_reserved_slots(ARGOF(t, 0)) + get_reserved_slots(ARGOF(t, 1)) + 1;
      if (FUNCTOROF(t) == notFunctor)
         return get_reserved_slots(ARGOF(t, 0)) + 1;
      if (FUNCTOROF(t) == notUnifiableFunctor)
         return get_reserved_slots(ARGOF(t, 0)) + 1;
      if (FUNCTOROF(t) == crossModuleCallFunctor)
         return get_reserved_slots(ARGOF(t, 1));

   }
   if (t == catchAtom)
      return 4; // One for each arg on the frame, plus an extra one for the choicepoint
   return 0;
}

typedef struct
{
   int size;
   int constant_count;
   Clause clause;
   int ip;
   int consp;
   int codep;
} compile_context_t;

void build_asm_context(void* c, instruction_t* i)
{
   compile_context_t* context = ((compile_context_t*)c);
   context->size += i->size;
   context->constant_count += i->constant_count;
}

void _assemble(void *p, instruction_t* i)
{
   compile_context_t* context = (compile_context_t*)p;
   context->clause->code[context->codep++] = i->opcode;
   if (i->constant_count != 0)
   {
      int index = context->consp++;
      assert(index < (1 << 16));
      context->clause->constants[index] = i->constant;
      context->clause->code[context->codep++] = (index >> 8) & 0xff;
      context->clause->code[context->codep++] = (index >> 0) & 0xff;
   }
   if (i->address != -1)
   {
      uintptr_t offset = i->address;
      if (sizeof(word) == 4)
      {
         context->clause->code[context->codep++] = (offset >> 24) & 0xff;
         context->clause->code[context->codep++] = (offset >> 16) & 0xff;
         context->clause->code[context->codep++] = (offset >>  8) & 0xff;
         context->clause->code[context->codep++] = (offset >>  0) & 0xff;
      }
      else if (sizeof(word) == 8)
      {
         context->clause->code[context->codep++] = (offset >> 56) & 0xff;
         context->clause->code[context->codep++] = (offset >> 48) & 0xff;
         context->clause->code[context->codep++] = (offset >> 40) & 0xff;
         context->clause->code[context->codep++] = (offset >> 32) & 0xff;
         context->clause->code[context->codep++] = (offset >> 24) & 0xff;
         context->clause->code[context->codep++] = (offset >> 16) & 0xff;
         context->clause->code[context->codep++] = (offset >>  8) & 0xff;
         context->clause->code[context->codep++] = (offset >>  0) & 0xff;
      }
      else if (sizeof(word) == 2)
      {
         context->clause->code[context->codep++] = (offset >>  8) & 0xff;
         context->clause->code[context->codep++] = (offset >>  0) & 0xff;
      }
      else
         assert(0 && "What kind of CPU IS this?");


   }
   if (i->slot != -1)
   {
      assert(i->slot < (1 << 16));
      context->clause->code[context->codep++] = (i->slot >> 8) & 0xff;
      context->clause->code[context->codep++] = (i->slot >> 0) & 0xff;
   }
}

Clause allocClause()
{
   Clause c = malloc(sizeof(clause));
   c->next = NULL;
   return c;
}

Clause assemble(instruction_list_t* instructions)
{
   compile_context_t context = {0, 0, NULL, 0, 0, 0};
   context.clause = allocClause();
   instruction_list_apply(instructions, &context, build_asm_context);
   context.clause->code = malloc(context.size);
   context.clause->constants = malloc(context.constant_count * sizeof(word));
#ifdef DEBUG
   context.clause->code_size = context.size;
   context.clause->constant_size = context.constant_count;
#endif
   instruction_list_apply(instructions, &context, _assemble);
   assert(context.codep == context.size);
   assert(context.consp == context.constant_count);

   return context.clause;
}


int compile_clause(word term, instruction_list_t* instructions, int* slot_count)
{
         //printf("Compiling "); PORTRAY(term); printf("\n");
   int arg_slots = 0; // Number of slots that will be used for passing the args of the predicate
   wmap_t variables = whashmap_new();
   if (TAGOF(term) == COMPOUND_TAG && FUNCTOROF(term) == clauseFunctor)
   {
      // Clause
      word head = ARGOF(term, 0);
      word body = ARGOF(term, 1);
      if (TAGOF(head) == COMPOUND_TAG)
         arg_slots = 0; // getConstant(FUNCTOROF(head), NULL).functor_data->arity;
      else
         arg_slots = 0;
      int local_cut_slots = get_reserved_slots(body);
      int next_slot = arg_slots;
      analyze_variables(head, 1, 0, variables, &next_slot, 0);
      analyze_variables(body, 0, 0, variables, &next_slot, 0);
      *slot_count = whashmap_length(variables) + local_cut_slots;
      compile_head(head, variables, instructions);
      push_instruction(instructions, INSTRUCTION(I_ENTER));
      int next_reserved = next_slot;
      int size = 0;
      if (!compile_body(body, variables, instructions, 1, &next_reserved, -1, &size))
      {
         whashmap_iterate(variables, free_varinfo, NULL);
         whashmap_free(variables);
         if (getException() == 0)
         {
            if (TAGOF(body) == COMPOUND_TAG && FUNCTOROF(body) == crossModuleCallFunctor)
               return type_error(callableAtom, ARGOF(body, 1));
            else
               return type_error(callableAtom, body);
         }
         return 0;
      }
   }
   else
   {
      // Fact
      if (TAGOF(term) == COMPOUND_TAG)
         arg_slots = 0; //getConstant(FUNCTOROF(term), NULL).functor_data->arity;
      else
         arg_slots = 0;
      analyze_variables(term, 1, 0, variables, &arg_slots, 0);
      *slot_count = arg_slots;
      compile_head(term, variables, instructions);
      push_instruction(instructions, INSTRUCTION(I_EXIT_FACT));
   }
   whashmap_iterate(variables, free_varinfo, NULL);
   whashmap_free(variables);
   return 1;
}

void set_variables(word variable, void* query)
{
   Query q = (Query)query;
   q->variables[q->variable_count++] = variable;
}

Query compile_query(word term)
{
   instruction_list_t instructions;
   List variables;
   init_list(&variables);
   init_instruction_list(&instructions);
   find_variables(term, &variables);
   int slot_count;
   if (!compile_clause(MAKE_VCOMPOUND(clauseFunctor, MAKE_LCOMPOUND(queryAtom, &variables), term), &instructions, &slot_count))
   {
      deinit_instruction_list(&instructions);
      free_list(&variables);
      return NULL;
   }
   Query q = malloc(sizeof(query));
   q->variable_count = 0;
   q->variables = malloc(sizeof(word) * list_length(&variables));
   list_apply(&variables, q, set_variables);
   q->clause = assemble(&instructions);
   q->clause->slot_count = slot_count;
   deinit_instruction_list(&instructions);
   free_list(&variables);
   return q;
}

void free_query(Query q)
{
   free(q->variables);
   free(q);
}

int qqqc = 0;

Clause compile_predicate_clause(word term, int with_choicepoint, char* meta)
{
   instruction_list_t instructions;
   init_instruction_list(&instructions);
   if (meta != NULL)
   {
      for (int i = 0; meta[i] != '\0'; i++)
      {
         if (!(meta[i] == '+' || meta[i] == '?' || meta[i] == '-'))
         {
            push_instruction(&instructions, INSTRUCTION_SLOT(S_QUALIFY, i));
         }
      }
   }
   if (with_choicepoint)
      push_instruction(&instructions, INSTRUCTION(TRY_ME_OR_NEXT_CLAUSE));
   int slot_count;
   if (!compile_clause(term, &instructions, &slot_count))
   {
      deinit_instruction_list(&instructions);
      return NULL;
   }
   Clause clause = assemble(&instructions);
   clause->slot_count = slot_count;
   deinit_instruction_list(&instructions);
   //PORTRAY(term); printf(" compiles to this:\n"); print_clause(clause); printf("--- end of compiled code\n");
   return clause;
}

struct predicate_context_t
{
   int length;
   int index;
   char* meta;
   Clause* next;
};

void _compile_predicate(word term, void* _context)
{
   struct predicate_context_t* context = (struct predicate_context_t*)_context;
   //printf("Compiling clause: "); PORTRAY(term); printf("\n");
   Clause q = compile_predicate_clause(DEREF(term), context->index + 1 < context->length, context->meta);
   context->index++;
   *(context->next) = q;
   context->next = &(q->next);
}

Clause compile_predicate(Predicate p)
{
   Clause clause = NULL;
   if (list_length(&p->clauses) == 0)
   {
      instruction_list_t instructions;
      init_instruction_list(&instructions);
      push_instruction(&instructions, INSTRUCTION(I_FAIL));
      clause = assemble(&instructions);
      deinit_instruction_list(&instructions);
   }
   else
   {
      Clause* ptr = &clause;
      struct predicate_context_t context;
      context.length = list_length(&p->clauses);
      context.index = 0;
      context.meta = p->meta;
      context.next = &clause;
      list_apply(&p->clauses, &context, _compile_predicate);
   }
   return clause;
}

Clause foreign_predicate_c(int(*func)(), int arity, int flags)
{
   instruction_list_t instructions;
   init_instruction_list(&instructions);
   if (flags & NON_DETERMINISTIC)
   {
      push_instruction(&instructions, INSTRUCTION_SLOT_ADDRESS(I_FOREIGN_NONDET, arity, (word)func));
      push_instruction(&instructions, INSTRUCTION(I_FOREIGN_RETRY));
   }
   else
      push_instruction(&instructions, INSTRUCTION_SLOT_ADDRESS(I_FOREIGN, arity, (word)func));
   Clause clause = assemble(&instructions);
   clause->slot_count = arity + ((flags & NON_DETERMINISTIC) != 0?1:0);
   deinit_instruction_list(&instructions);
   return clause;
}

Clause foreign_predicate_js(word func, int arity, int flags)
{
   instruction_list_t instructions;
   init_instruction_list(&instructions);
   push_instruction(&instructions, INSTRUCTION_SLOT_ADDRESS(I_FOREIGN_JS, arity, func));
   push_instruction(&instructions, INSTRUCTION(I_FOREIGN_JS_RETRY));
   Clause clause = assemble(&instructions);
   clause->slot_count = arity + ((flags & NON_DETERMINISTIC) != 0?1:0);
   // FIXME: set slot_count!
   deinit_instruction_list(&instructions);
   return clause;
}
