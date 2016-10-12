#ifndef _MODULE_H
#define _MODULE_H

#include "types.h"
typedef struct
{
   List clauses;
   Clause firstClause;
   char* meta;
   int flags;

} predicate;

typedef predicate* Predicate;

Predicate lookup_predicate(Module module, word functor);
Module create_module(word name);
void destroy_module(Module m);
Module find_module(word name);
void add_clause(Module module, word functor, word clause);
void free_clause(Clause c);
void initialize_modules();
int define_foreign_predicate_c(Module module, word functor, int(*func)(), int flags);
int set_meta(Module module, word functor, char* meta);
int set_dynamic(Module module, word functor);
int asserta(Module, word);
int assertz(Module, word);
int abolish(Module, word);
void retract(Module, word);


#endif
