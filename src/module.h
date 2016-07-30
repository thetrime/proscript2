#include "types.h"
Predicate lookup_predicate(Module module, word functor);
Module create_module(word name);
Module find_module(word name);
void add_clause(Module module, word functor, word clause);
void initialize_modules();
int define_foreign_predicate_c(Module module, word functor, int(*func)(), int flags);
