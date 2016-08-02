#ifdef EMSCRIPTEN
#include <emscripten/emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include "module.h"
#include "kernel.h"
#include "whashmap.h"
#include "ctable.h"
#include "compiler.h"
#include <stdio.h>

wmap_t modules = NULL;

void initialize_modules()
{
   modules = whashmap_new();
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
      //printf("Adding clause to existing predicate of "); PORTRAY(module->name); printf(":"); PORTRAY(functor); printf("\n");
      (*(p->tail)) = malloc(sizeof(predicate_cell_t));
      (*(p->tail))->term = clause;
      (*(p->tail))->next = NULL;
      p->tail = &((*p->tail)->next);
   }
   else
   {
   //printf("Adding clause to new predicate "); PORTRAY(module->name); printf(":"); PORTRAY(functor); printf("(%lu, %p)\n", functor, module);
      p = malloc(sizeof(predicate));
      p->head = malloc(sizeof(predicate_cell_t));
      p->head->term = clause;
      p->head->next = NULL;
      p->meta = NULL;
      p->tail = &(p->head->next);
      p->firstClause = NULL;
      whashmap_put(module->predicates, functor, p);
   }
}
