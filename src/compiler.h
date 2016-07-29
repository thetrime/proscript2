#include "types.h"
#include "list.h"

Query compile_query(word);
void free_query(Query);
Clause compile_predicate(Predicate p);
void find_variables(word term, List* list);
