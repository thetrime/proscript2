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
#include <stdio.h>

wmap_t modules = NULL;

void initialize_modules()
{
   modules = whashmap_new();
}

void free_predicate(Predicate p)
{
   printf("FIXME: Predicate is not freed\n");
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
   p->firstClause = foreign_predicate_c(func, getConstant(functor).data.functor_data->arity, flags);
   whashmap_put(module->predicates, functor, p);
   return 1;
}

EMSCRIPTEN_KEEPALIVE
int define_foreign_predicate_js(Module module, word functor, word func)
{
   Predicate p;
   if (whashmap_get(module->predicates, functor, (any_t)&p) == MAP_OK)
      return 0;
   //printf("Defining foreign (JS) predicate "); PORTRAY(module->name); printf(":"); PORTRAY(functor); printf("\n");
   p = malloc(sizeof(predicate));
   p->meta = NULL;
   p->flags = PREDICATE_FOREIGN;
   p->firstClause = foreign_predicate_js(func);
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

Module create_module(word name)
{
   Module m = malloc(sizeof(module));
   m->name = name;
   whashmap_put(modules, name, m);
   m->predicates = whashmap_new();
   return m;
}

EMSCRIPTEN_KEEPALIVE
Module find_module(word name)
{
   Module m;
   if (whashmap_get(modules, name, (any_t)&m) == MAP_OK)
      return m;
   return NULL;
}

void add_clause(Module module, word functor, word clause)
{
   Predicate p;
   if (whashmap_get(module->predicates, functor, (any_t)&p) == MAP_OK)
   {
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
      whashmap_put(module->predicates, functor, p);
   }
}


int asserta(Module module, word clause)
{
   word functor = clause_functor(clause);
   Predicate p;
   if (whashmap_get(module->predicates, functor, (any_t)&p) == MAP_OK)
   {
      if ((p->flags & PREDICATE_DYNAMIC) == 0)
         return permission_error(modifyAtom, staticProcedureAtom, predicate_indicator(clause));
      // Existing predicate. Put the new clause at the start
      list_unshift(&p->clauses, clause);
   }
   else
   {
      // New predicate
      p = malloc(sizeof(predicate));
      p->flags = PREDICATE_DYNAMIC;
      init_list(&p->clauses);
      list_unshift(&p->clauses, clause);
      p->meta = NULL;
      p->firstClause = NULL;
      whashmap_put(module->predicates, functor, p);
   }
   p->firstClause = compile_predicate(p);
   return 1;
}

int assertz(Module module, word clause)
{
   word functor = clause_functor(clause);
   Predicate p;
   if (whashmap_get(module->predicates, functor, (any_t)&p) == MAP_OK)
   {
      if ((p->flags & PREDICATE_DYNAMIC) == 0)
         return permission_error(modifyAtom, staticProcedureAtom, predicate_indicator(clause));
      // Existing predicate. Put the new clause at the start
      list_append(&p->clauses, clause);
   }
   else
   {
      // New predicate
      p = malloc(sizeof(predicate));
      p->flags = PREDICATE_DYNAMIC;
      init_list(&p->clauses);
      list_unshift(&p->clauses, clause);
      p->meta = NULL;
      p->firstClause = NULL;
      whashmap_put(module->predicates, functor, p);
   }
   p->firstClause = compile_predicate(p);
   return 1;
}

int abolish(Module module, word indicator)
{
   if (!must_be_predicate_indicator(indicator))
   {
      return ERROR;
   }
   word functor = MAKE_FUNCTOR(ARGOF(indicator, 0), getConstant(ARGOF(indicator, 1)).data.integer_data->data);
   Predicate p = lookup_predicate(module, functor);
   if ((p->flags & PREDICATE_DYNAMIC) == 0)
      return permission_error(modifyAtom, staticProcedureAtom, indicator);
   whashmap_remove(module->predicates, functor);
   free_predicate(p);
   return SUCCESS;
}

void retract(Module module, word clause)
{
   word functor = clause_functor(clause);
   Predicate p;
   if (whashmap_get(module->predicates, functor, (any_t)&p) == MAP_OK)
   {
      // Retract the *first* value for clause that matches
      list_delete_first(&p->clauses, DEREF(clause));
      p->firstClause = compile_predicate(p);
   }
}
