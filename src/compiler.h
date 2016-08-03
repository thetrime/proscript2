#include "types.h"
#include "list.h"
#include "module.h"

Query compile_query(word);
void free_query(Query);
Clause compile_predicate(Predicate p);
void find_variables(word term, List* list);
Clause foreign_predicate_js(word func);
Clause foreign_predicate_c(int(*func)(), int arity, int flags);
