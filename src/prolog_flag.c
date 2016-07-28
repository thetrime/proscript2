#include "hashmap.h"
#include "constants.h"

map_t flags = NULL;

void initialize_prolog_flags()
{
   flags = hashmap_new();
   hashmap_put(flags, "double_quotes", (any_t)codesAtom);
   hashmap_put(flags, "debug", (any_t)falseAtom);
   hashmap_put(flags, "unknown", (any_t)errorAtom);
   hashmap_put(flags, "char_conversion", (any_t)falseAtom);
}

word get_prolog_flag(char* name)
{
   if (flags == NULL)
      initialize_prolog_flags();
   word w;
   if (hashmap_get(flags, name, (any_t*)&w) == MAP_OK)
      return w;
   return 0;
}
