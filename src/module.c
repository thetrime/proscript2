#include "global.h"
#ifdef EMSCRIPTEN
#include <emscripten/emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include "module.h"
#include "errors.h"
#include "constants.h"
#include "list.h"
#include "kernel.h"
#include "whashmap.h"
#include "ctable.h"
#include "compiler.h"
#include "checks.h"
#include "local.h"
#include <stdio.h>
#include <assert.h>

wmap_t modules = NULL;

void initialize_modules()
{
   modules = whashmap_new();
}

void free_clause(Clause c)
{
   if (c->code != NULL)
      free(c->code);
   if (c->constants != NULL)
   {
      printf("Releasing constants from freed clause\n");
      for (int i = 0; i < c->constant_size; i++)
         release_constant("freed clause constant", c->constants[i]);
      free(c->constants);
      printf("... done\n");
   }
   free(c);
}

void free_clauses(Clause c)
{
   while (c != NULL)
   {
      Clause next = c->next;
      free_clause(c);
      c = next;
   }
}

void free_predicate(Predicate p)
{
   free_clauses(p->firstClause);
   if ((p->flags & PREDICATE_FOREIGN) == 0)
      free_list(&p->clauses);
   free(p);
}

int define_foreign_predicate_c(Module module, word functor, int(*func)(), int flags)
{
   Predicate p;
   if (whashmap_get(module->predicates, functor, (any_t)&p) == MAP_OK)
      return 0;
   //printf("Defining foreign (C) predicate "); PORTRAY(module->name); printf(":"); PORTRAY(functor); printf(" as %p\n", func);
   p = malloc(sizeof(predicate));
   p->meta = NULL;
   p->flags = PREDICATE_FOREIGN;
   p->firstClause = foreign_predicate_c(func, getConstant(functor, NULL).functor_data->arity, flags);
   acquire_constant("predicate name", functor);
   whashmap_put(module->predicates, functor, p);
   return 1;
}

EMSCRIPTEN_KEEPALIVE
int define_foreign_predicate_js(Module module, word functor, word func)
{
   Predicate p;
   if (whashmap_get(module->predicates, functor, (any_t)&p) == MAP_OK)
   {
      printf("Warning: Redefining system predicate "); PORTRAY(module->name); printf(":"); PORTRAY(functor); printf(" with Javascript version\n");
   }
   //printf("Defining foreign (JS) predicate "); PORTRAY(module->name); printf(":"); PORTRAY(functor); printf("\n");
   p = malloc(sizeof(predicate));
   p->meta = NULL;
   p->flags = PREDICATE_FOREIGN;
   Functor f = getConstant(functor, NULL).functor_data;
   p->firstClause = foreign_predicate_js(func, f->arity, NON_DETERMINISTIC);
   acquire_constant("predicate name", functor);
   whashmap_put(module->predicates, functor, p);
   return 1;
}


Predicate lookup_predicate(Module module, word functor)
{
   Predicate p;
   if (whashmap_get(module->predicates, functor, (any_t)&p) == MAP_OK)
      return p;
   //printf("Unable to find "); PORTRAY(module->name); printf(":"); PORTRAY(functor); printf("\n");
   return NULL;
}

static
int _destroy_module_predicate(any_t ignored, word functor, any_t predicate)
{
   free_predicate((Predicate)predicate);
   return MAP_OK;
}

void destroy_module(Module m)
{
   printf("Destroying module "); PORTRAY(m->name); printf("\n");
   // CHECKME: Could there be references to this module or the predicates elsewhere?
   whashmap_iterate(m->predicates, _destroy_module_predicate, NULL);
   whashmap_free(m->predicates);
   free(m);
}

Module create_module(word name)
{
   Module m = malloc(sizeof(module));
   Module old_module = NULL;
   m->name = name;
   whashmap_put(modules, name, m);
   m->predicates = whashmap_new();
   return m;
   //printf("Created a module: "); PORTRAY(name); printf("\n");
}

EMSCRIPTEN_KEEPALIVE
Module find_module(word name)
{
   Module m;
   if (whashmap_get(modules, name, (any_t)&m) == MAP_OK)
      return m;
//   printf("Could not find module "); PORTRAY(name); printf("\n");
   return NULL;
}

int set_meta(Module module, word functor, char* meta)
{
   Predicate p;
   if (whashmap_get(module->predicates, functor, (any_t)&p) == MAP_OK)
   {
      p->meta = meta;
   }
   else
   {
      p = malloc(sizeof(predicate));
      init_list(&p->clauses);
      p->flags = 0;
      p->meta = meta;
      p->firstClause = NULL;
      acquire_constant("predicate name", functor);
      whashmap_put(module->predicates, functor, p);
   }
   return 1;
}

int set_dynamic(Module module, word functor)
{
   Predicate p;
   if (whashmap_get(module->predicates, functor, (any_t)&p) == MAP_OK)
   {
      p->flags = PREDICATE_DYNAMIC;
   }
   else
   {
      p = malloc(sizeof(predicate));
      init_list(&p->clauses);
      p->flags = PREDICATE_DYNAMIC;
      p->meta = NULL;
      p->firstClause = NULL;
      acquire_constant("predicate name", functor);
      whashmap_put(module->predicates, functor, p);
   }
   return 1;
}

void _release_uncompiled_constants(word w, void* ignored)
{
   forall_term_constants(w, "uncompiled constant", release_constant);
}

// FIXME: This is a bad idea. We should only do this when the predicate (or clause) is deleted!
void release_uncompiled_constants(Predicate p)
{
   list_apply(&p->clauses, NULL, _release_uncompiled_constants);
}

void add_clause(Module module, word functor, word clause)
{
   Predicate p;
   printf("Adding clause to "); PORTRAY(functor); printf("\n");
   // See asserta for an explanaton of what is going on here.
   word* local;
   copy_local(clause, &local);
   clause = (word)local;
   // I realised that (consult('file.pl'), fail ; some_fact_in_file(X)) results in a crash
   // because when we backtrack to before the consult/1 all the term references created in
   // consult/1 are destroyed, but we still have references to them in the list of clauses
   // in the predicate. To fix this, we now keep a local copy of every clause. This means
   // that if you ever want to unconsult (or reconsult) a file, you must remember to free
   // the old references or you'll leak memory.

   // When we add a clause, we do not actually compile it - instead we wait until it is needed.
   // This means that if you consult a file, then run AGC, we will think all the constants
   // the parser found are garbage and free them. Then, when the code is executed, it will
   // contain references to deleted constants.
   // To fix this, we acquire every constant in the clause right away, and release them
   // once we compile the clauses
   forall_term_constants(clause, "uncompiled clause", acquire_constant);

   if (whashmap_get(module->predicates, functor, (any_t)&p) == MAP_OK)
   {
      if ((p->flags & PREDICATE_FOREIGN) != 0)
      {
         printf("Warning: Attempt to redefine foreign predicate "); PORTRAY(functor); printf(" with prolog version. Ignoring Prolog\n");
         free_local((word)local);
         return;
      }
      list_append(&p->clauses, clause);
   }
   else
   {
      p = malloc(sizeof(predicate));
      init_list(&p->clauses);
      list_append(&p->clauses, clause);
      p->flags = 0;
      p->meta = NULL;
      p->firstClause = NULL;
      acquire_constant("predicate name", functor);
      whashmap_put(module->predicates, functor, p);
   }
}


int asserta(Module module, word clause)
{
   word functor;
   if (!clause_functor(clause, &functor))
      return ERROR;
   Predicate p;
   word* local;

   // This bears some explaining. copy_local will allocate some space on the heap and copy the term there to ensure it is safe
   // even in the event of backtracking mucking up the global stack. However, we now have 2 things to keep track of - the (copied)
   // clause and the memory that we must free. Fortunately, there is a trick here - because variables in Prolog are just pointers
   // if we cast local to a word, then we have a variable that is pre-bound to the copied term
   // This means that DEREF(local) will be a clause, but we can still free_local(local) later.
   // Also note that while you can just free(local), the constant references in the copy will not be freed
   // Note that this ONLY applies for copy_local, and not copy_local_with_extra_space!
   copy_local(clause, &local);
   clause = (word)local;

   struct cell_t* cell;
   if (whashmap_get(module->predicates, functor, (any_t)&p) == MAP_OK)
   {
      //printf("Assert has found a clause for "); PORTRAY(functor); printf(" already exists\n");
      if ((p->flags & PREDICATE_DYNAMIC) == 0)
      {
         free_local((word)local);
         return permission_error(modifyAtom, staticProcedureAtom, predicate_indicator(functor));
      }
      // Existing predicate. Put the new clause at the start
      cell = list_unshift(&p->clauses, clause);
   }
   else
   {
      // New predicate
      p = malloc(sizeof(predicate));
      p->flags = PREDICATE_DYNAMIC;
      init_list(&p->clauses);
      cell = list_unshift(&p->clauses, clause);
      p->meta = NULL;
      p->firstClause = NULL;
      acquire_constant("predicate name", functor);
      whashmap_put(module->predicates, functor, p);
   }
   free_clauses(p->firstClause);
   p->firstClause = compile_predicate(p);
   //printf("Assert complete: %p\n", p->firstClause);
   if (p->firstClause == NULL)
   {
      // Compilation failed. Scrub out that clause
      free_local((word)local);
      list_splice(&p->clauses, cell);
      return ERROR;
   }
   return SUCCESS;
}

int assertz(Module module, word clause)
{
   word functor;
   if (!clause_functor(clause, &functor))
      return ERROR;
   Predicate p;
   word* local;
   struct cell_t* cell;
   copy_local(clause, &local);
   clause = (word)local; // See asserta for an explanation of what is happening here

   if (whashmap_get(module->predicates, functor, (any_t)&p) == MAP_OK)
   {
      if ((p->flags & PREDICATE_DYNAMIC) == 0)
      {
         free_local((word)local);
         return permission_error(modifyAtom, staticProcedureAtom, predicate_indicator(functor));
      }
      // Existing predicate. Put the new clause at the start
      cell = list_append(&p->clauses, clause);
   }
   else
   {
      // New predicate
      p = malloc(sizeof(predicate));
      p->flags = PREDICATE_DYNAMIC;
      init_list(&p->clauses);
      cell = list_append(&p->clauses, clause);
      p->meta = NULL;
      p->firstClause = NULL;
      acquire_constant("predicate name", functor);
      whashmap_put(module->predicates, functor, p);
   }
   free_clauses(p->firstClause);
   p->firstClause = compile_predicate(p);
   if (p->firstClause == NULL)
   {
      // Compilation failed. Scrub out that clause
      free_local((word)local);
      list_splice(&p->clauses, cell);
      return ERROR;
   }
   return SUCCESS;
}

void _free_asserted_terms(word term, void* ignored)
{
   if (term != 0)
      free((void*)term);
}

int abolish(Module module, word indicator)
{
   if (!must_be_predicate_indicator(indicator))
   {
      return ERROR;
   }
   word functor = MAKE_FUNCTOR(ARGOF(indicator, 0), getConstant(ARGOF(indicator, 1), NULL).integer_data);
   Predicate p = lookup_predicate(module, functor);
   if ((p->flags & PREDICATE_DYNAMIC) == 0)
      return permission_error(modifyAtom, staticProcedureAtom, indicator);
   whashmap_remove(module->predicates, functor);
   list_apply(&p->clauses, NULL, _free_asserted_terms);
   free_predicate(p);
   return SUCCESS;
}

void retract(Module module, word clause)
{
   word functor;
   if (!clause_functor(clause, &functor))
   {
      CLEAR_EXCEPTION();
      return;
   }
   Predicate p;
   if (whashmap_get(module->predicates, functor, (any_t)&p) == MAP_OK)
   {
      // Retract the *first* value for clause that unifies
      word deleted_term = list_delete_first(&p->clauses, DEREF(clause));
      if (deleted_term != 0)
         free((void*)deleted_term);
      free_clauses(p->firstClause);
      p->firstClause = compile_predicate(p);
   }
}
