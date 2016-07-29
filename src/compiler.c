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

typedef struct
{
   word variable;
   int fresh;
   int slot;
} var_info_t;

struct instruction_t
{
   struct instruction_t* next;
   unsigned char opcode;
   word constant;
   int slot;
   long address;
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
   for (instruction_t* cell = list->head; cell; cell = cell->next)
      fn(data, cell);
}

void push_instruction(instruction_list_t* list, instruction_t* i)
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

instruction_t* INSTRUCTION_SLOT_ADDRESS(unsigned char opcode, int slot, int address)
{
   instruction_t* i = malloc(sizeof(instruction_t));
   i->opcode = opcode;
   i->constant = (word)-1;
   i->slot = slot;
   i->address = address;
   i->size = 3;
   i->constant_count = 0;
   return i;
}

instruction_t* INSTRUCTION_ADDRESS(unsigned char opcode, int address)
{
   instruction_t* i = malloc(sizeof(instruction_t));
   i->opcode = opcode;
   i->constant = (word)-1;
   i->slot = -1;
   i->address = address;
   i->size = 3;
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

int compile_argument(word arg, wmap_t variables, instruction_list_t* instructions, int embedded)
{
   if (TAGOF(arg) == VARIABLE_TAG)
   {
      var_info_t* varinfo;
      assert(whashmap_get(variables, arg, (any_t)&varinfo) == MAP_OK);
      if (varinfo->fresh)
      {
         varinfo->fresh = 0;
         if (embedded)
         {
            push_instruction(instructions, INSTRUCTION_SLOT(H_FIRSTVAR, varinfo->slot));
            return 0;
         }
         else
         {
            push_instruction(instructions, INSTRUCTION(H_VOID));
            return 1;
         }
      }
      else
      {
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
      Functor f = getConstant(FUNCTOROF(arg)).data.functor_data;
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
      Functor f = getConstant(FUNCTOROF(term)).data.functor_data;
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

void compile_term_creation(word term, wmap_t variables, instruction_list_t* instructions)
{
   if (TAGOF(term) == VARIABLE_TAG)
   {
      var_info_t* varinfo;
      assert(whashmap_get(variables, term, (any_t)&varinfo) == MAP_OK);
      if (varinfo->fresh)
      {
         push_instruction(instructions, INSTRUCTION_SLOT(B_FIRSTVAR, varinfo->slot));
         varinfo->fresh = 0;
      }
      else if (0)
      {
         push_instruction(instructions, INSTRUCTION_SLOT(B_ARGVAR, varinfo->slot));
      }
      else
      {
         push_instruction(instructions, INSTRUCTION_SLOT(B_VAR, varinfo->slot));
      }
   }
   else if (TAGOF(term) == COMPOUND_TAG)
   {
      push_instruction(instructions, INSTRUCTION_CONST(B_FUNCTOR, FUNCTOROF(term)));
      Functor f = getConstant(FUNCTOROF(term)).data.functor_data;
      for (int i = 0; i < f->arity; i++)
         compile_term_creation(ARGOF(term, i), variables, instructions);
      push_instruction(instructions, INSTRUCTION(B_POP));
   }
   else if (TAGOF(term) == CONSTANT_TAG)
   {
      push_instruction(instructions, INSTRUCTION_CONST(B_ATOM, term));
   }
   else
   {
      assert(0 && "Illegal tag");
   }
}

int compile_body(word term, wmap_t variables, instruction_list_t* instructions, int is_tail, int* next_reserved, int local_cut)
{
   int rc = 1;
   term = DEREF(term);
   if (TAGOF(term) == VARIABLE_TAG)
   {
      compile_term_creation(term, variables, instructions);
      push_instruction(instructions, INSTRUCTION(I_USERCALL));
      if (is_tail)
         push_instruction(instructions, INSTRUCTION(I_EXIT));
   }
   else if (term == cutAtom)
   {
      if (local_cut != -1)
         push_instruction(instructions, INSTRUCTION_SLOT(C_LCUT, local_cut));
      else
         push_instruction(instructions, INSTRUCTION(I_CUT));
      if (is_tail)
         push_instruction(instructions, INSTRUCTION(I_EXIT));
   }
   else if (term == trueAtom)
   {
      if (is_tail)
         push_instruction(instructions, INSTRUCTION(I_EXIT));
   }
   else if (term == catchAtom)
   {
      int slot = (*next_reserved)++;
      push_instruction(instructions, INSTRUCTION_SLOT(I_CATCH, slot));
      push_instruction(instructions, INSTRUCTION_SLOT(I_EXITCATCH, slot));
   }
   else if (term == failAtom)
   {
      push_instruction(instructions, INSTRUCTION(I_FAIL));
   }
   else if (TAGOF(term) == CONSTANT_TAG)
   {
      constant c = getConstant(term);
      if (c.type == ATOM_TYPE)
      {
         push_instruction(instructions, INSTRUCTION_CONST(is_tail?I_DEPART:I_CALL, MAKE_FUNCTOR(term, 0)));
      }
      else
      {
         printf("Failure - cannot call this\n");
         assert(0);
      }
   }
   else if (TAGOF(term) == COMPOUND_TAG)
   {
      if (FUNCTOROF(term) == conjunctionFunctor)
      {
         rc &= compile_body(ARGOF(term,0), variables, instructions, 0, next_reserved, local_cut);
         rc &= compile_body(ARGOF(term,1), variables, instructions, is_tail, next_reserved, local_cut);
      }
      else  if (FUNCTOROF(term) == disjunctionFunctor)
      {
         if (TAGOF(ARGOF(term, 0)) == COMPOUND_TAG && FUNCTOROF(ARGOF(term,0)) == localCutFunctor)
         {
            // if-then-else
            int cut_point = (*next_reserved)++;
            instruction_t* if_then_else = INSTRUCTION_SLOT_ADDRESS(C_IF_THEN_ELSE, cut_point, -1);
            push_instruction(instructions, if_then_else);
            // If
            rc &= compile_body(ARGOF(ARGOF(term,0),0), variables, instructions, 0, next_reserved, cut_point);
            // (Cut)
            push_instruction(instructions, INSTRUCTION_SLOT(C_CUT, cut_point));
            // Then
            rc &= compile_body(ARGOF(ARGOF(term,0),1), variables, instructions, 0, next_reserved, cut_point);
            // (and now jump out before the Else)
            instruction_t* jump = INSTRUCTION_ADDRESS(C_JUMP, -1);
            push_instruction(instructions, jump);
            if_then_else->address = instruction_count(instructions);
            rc &= compile_body(ARGOF(term, 1), variables, instructions, 0, next_reserved, local_cut);
            jump->address = instruction_count(instructions);
         }
         else
         {
            // ordinary disjunction
            instruction_t* or = INSTRUCTION_ADDRESS(C_OR, -1);
            push_instruction(instructions, or);
            rc &= compile_body(ARGOF(term, 0), variables, instructions, 0, next_reserved, local_cut);
            instruction_t* jump = INSTRUCTION_ADDRESS(C_JUMP, -1);
            or->address = instruction_count(instructions);
            rc &= compile_body(ARGOF(term, 1), variables, instructions, is_tail, next_reserved, local_cut);
            jump->address = instruction_count(instructions);
         }
         if (is_tail)
            push_instruction(instructions, INSTRUCTION(I_EXIT));
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
         instruction_t* if_then_else = INSTRUCTION_SLOT_ADDRESS(C_IF_THEN_ELSE, cut_point, -1);
         push_instruction(instructions, if_then_else);
         // If
         rc &= compile_body(ARGOF(term, 0), variables, instructions, 0, next_reserved, cut_point);
         // (cut)
         push_instruction(instructions, INSTRUCTION_SLOT(C_CUT, cut_point));
         // Then
         push_instruction(instructions, INSTRUCTION(I_FAIL));
         // Else
         if_then_else->address = instruction_count(instructions);
         if (is_tail)
            push_instruction(instructions, INSTRUCTION(I_EXIT));
      }
      else if (FUNCTOROF(term) == throwFunctor)
      {
         compile_term_creation(ARGOF(term, 0), variables, instructions);
         push_instruction(instructions, INSTRUCTION(B_THROW));
      }
      else if (FUNCTOROF(term) == crossModuleCallFunctor)
      {
         push_instruction(instructions, INSTRUCTION_CONST(I_SWITCH_MODULE, ARGOF(term, 0)));
         rc &= compile_body(ARGOF(term, 1), variables, instructions, 0, next_reserved, local_cut);
         push_instruction(instructions, INSTRUCTION(I_EXITMODULE));
         if (is_tail)
            push_instruction(instructions, INSTRUCTION(I_EXIT));
      }
      else if (FUNCTOROF(term) == cleanupChoicepointFunctor)
      {
         compile_term_creation(ARGOF(term, 0), variables, instructions);
         compile_term_creation(ARGOF(term, 1), variables, instructions);
         push_instruction(instructions, INSTRUCTION(B_CLEANUP_CHOICEPOINT));
      }
      else if (FUNCTOROF(term) == localCutFunctor)
      {
         int cut_point = (*next_reserved)++;
         push_instruction(instructions, INSTRUCTION_SLOT(C_IF_THEN, cut_point));
         rc &= compile_body(ARGOF(term, 0), variables, instructions, 0, next_reserved, cut_point);
         push_instruction(instructions, INSTRUCTION_SLOT(C_CUT, cut_point));
         rc &= compile_body(ARGOF(term, 1), variables, instructions, 0, next_reserved, local_cut);
         if (is_tail)
            push_instruction(instructions, INSTRUCTION(I_EXIT));
      }
      else if (FUNCTOROF(term) == notUnifiableFunctor)
      {
         rc &= compile_body(MAKE_VCOMPOUND(notFunctor, MAKE_VCOMPOUND(unifyFunctor, ARGOF(term, 0), ARGOF(term, 1))), variables, instructions, is_tail, next_reserved, local_cut);
      }
      else if (FUNCTOROF(term) == unifyFunctor)
      {
         compile_term_creation(ARGOF(term, 0), variables, instructions);
         compile_term_creation(ARGOF(term, 1), variables, instructions);
         push_instruction(instructions, INSTRUCTION(I_UNIFY));
         if (is_tail)
            push_instruction(instructions, INSTRUCTION(I_EXIT));
      }
      else
      {
         Functor f = getConstant(FUNCTOROF(term)).data.functor_data;
         for (int i = 0; i < f->arity; i++)
            compile_term_creation(ARGOF(term, i), variables, instructions);
         push_instruction(instructions, INSTRUCTION_CONST(is_tail?I_DEPART:I_EXIT, FUNCTOROF(term)));
      }
   }
   else
   {
      rc = 0;
   }
   return rc;
}

var_info_t* VarInfo(word variable, int fresh, int slot)
{
   var_info_t* v = malloc(sizeof(var_info_t));
   v->variable = variable;
   v->fresh = fresh;
   v->slot = slot;
   return v;
}

void find_variables(word term, List* list)
{
   term = DEREF(term);
   if (TAGOF(term) == VARIABLE_TAG && !list_contains(list, term))
      list_append(list, term);
   else if (TAGOF(term) == COMPOUND_TAG)
   {
      Functor f = getConstant(FUNCTOROF(term)).data.functor_data;
      for (int i = 0; i < f->arity; i++)
         find_variables(ARGOF(term, i), list);
   }
}


int analyze_variables(word term, int is_head, int depth, wmap_t map, int* next_slot)
{
   int rc = 0;
   if (TAGOF(term) == VARIABLE_TAG)
   {
      var_info_t* varinfo;
      if (whashmap_get(map, term, (any_t)&varinfo) == MAP_MISSING)
      {
         varinfo = VarInfo(term, 1, (*next_slot)++);
         whashmap_put(map, term, varinfo);
         rc++;
      }
   }
   else if (TAGOF(term) == COMPOUND_TAG)
   {
      Functor f = getConstant(FUNCTOROF(term)).data.functor_data;
      if (is_head && depth == 0)
      {
         for (int i = 0; i < f->arity; i++)
         {
            if (TAGOF(ARGOF(term, i)) == VARIABLE_TAG)
            {
               var_info_t* varinfo;
               if (whashmap_get(map, ARGOF(term, i), (any_t)&varinfo) == MAP_MISSING)
               {
                  varinfo = VarInfo(ARGOF(term, i), 1, i);
                  whashmap_put(map, ARGOF(term,i), varinfo);
               }
            }
            else
               rc += analyze_variables(ARGOF(term, i), is_head, depth+1, map, next_slot);
         }
      }
      else
      {
         for (int i = 0; i < f->arity; i++)
               rc += analyze_variables(ARGOF(term, i), is_head, depth+1, map, next_slot);
      }
   }
   return rc;
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
   }
   if (t == catchAtom)
      return 1;
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
      context->clause->code[context->codep++] = (i->address >> 24) & 0xff;
      context->clause->code[context->codep++] = (i->address >> 16) & 0xff;
      context->clause->code[context->codep++] = (i->address >>  8) & 0xff;
      context->clause->code[context->codep++] = (i->address >>  0) & 0xff;
   }
   if (i->slot != -1)
   {
      assert(i->slot < (1 << 16));
      context->clause->code[context->codep++] = (i->slot >> 8) & 0xff;
      context->clause->code[context->codep++] = (i->slot >> 0) & 0xff;
   }
   // FIXME: Foreign functions not implemented
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

void free_varinfo(void* ignored, word key, void* value)
{
   free(value);
}

int compile_clause(word term, instruction_list_t* instructions)
{
//   printf("Compiling "); PORTRAY(term); printf("\n");
   int arg_slots = 0; // Number of slots that will be used for passing the args of the predicate
   wmap_t variables = whashmap_new();
   if (TAGOF(term) == COMPOUND_TAG && FUNCTOROF(term) == clauseFunctor)
   {
      // Clause
      word head = ARGOF(term, 0);
      word body = ARGOF(term, 1);
      if (TAGOF(head) == COMPOUND_TAG)
         arg_slots = getConstant(FUNCTOROF(head)).data.functor_data->arity;
      else
         arg_slots = 0;
      int local_cut_slots = get_reserved_slots(body);
      int next_slot = arg_slots + local_cut_slots;
      analyze_variables(head, 1, 0, variables, &next_slot);
      analyze_variables(body, 0, 1, variables, &next_slot);
      compile_head(head, variables, instructions);
      push_instruction(instructions, INSTRUCTION(I_ENTER));
      int next_reserved = 0;

      if (!compile_body(body, variables, instructions, 1, &next_reserved, -1))
      {
         whashmap_free(variables);
         if (TAGOF(body) == COMPOUND_TAG && FUNCTOROF(body) == crossModuleCallFunctor)
            return type_error(callableAtom, ARGOF(body, 1));
         else
            return type_error(callableAtom, body);
      }
   }
   else
   {
      // Fact
      if (TAGOF(term) == COMPOUND_TAG)
         arg_slots = getConstant(FUNCTOROF(term)).data.functor_data->arity;
      else
         arg_slots = 0;
      analyze_variables(term, 1, 0, variables, &arg_slots);
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
   compile_clause(MAKE_VCOMPOUND(clauseFunctor, MAKE_LCOMPOUND(queryAtom, &variables), term), &instructions);
   Query q = malloc(sizeof(query));
   q->variable_count = 0;
   q->variables = malloc(sizeof(word) * list_length(&variables));
   list_apply(&variables, q, set_variables);
   q->clause = assemble(&instructions);
   deinit_instruction_list(&instructions);
   free_list(&variables);
   return q;
}

void free_query(Query q)
{
   free(q->variables);
   free(q);
}

Clause compile_predicate_clause(word term, int with_choicepoint, char* meta)
{
   instruction_list_t instructions;
   init_instruction_list(&instructions);
   if (meta != NULL)
   {
      for (int i = 0; meta[i] != '\0'; i++)
      {
         if (!(meta[i] == '+' || meta[i] == '?' || meta[i] == '-'))
            push_instruction(&instructions, INSTRUCTION_SLOT(S_QUALIFY, i));
      }
   }
   if (with_choicepoint)
      push_instruction(&instructions, INSTRUCTION(TRY_ME_OR_NEXT_CLAUSE));
   compile_clause(term, &instructions);
   Clause clause = assemble(&instructions);
   deinit_instruction_list(&instructions);
   //printf("Just compiled this:\n");
   //print_clause(clause);
   return clause;
}

Clause compile_predicate(Predicate p)
{
   Clause clause = NULL;
   if (p->head == NULL)
   {
      instruction_list_t instructions;
      init_instruction_list(&instructions);
      push_instruction(&instructions, INSTRUCTION(I_FAIL));
      clause = assemble(&instructions);
      deinit_instruction_list(&instructions);
   }
   else
   {
      predicate_cell_t* cell = p->head;
      Clause* ptr = &clause;
      while(cell != NULL)
      {
         Clause q = compile_predicate_clause(cell->term, cell->next != NULL, p->meta);
         *ptr = q;
         ptr = &((*ptr)->next);
         cell = cell->next;
      };
   }
   return clause;
}
