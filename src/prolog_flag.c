#include "hashmap.h"
#include "constants.h"
#include "kernel.h"
#include "ctable.h"
#include "list.h"
#include "errors.h"


typedef struct
{
   char* name;
   int (*set)(word name, word value);
   word value;
} prolog_flag_t;

int static_flag(word name, word value)
{
   return SET_EXCEPTION(permission_error(modifyAtom, flagAtom, name));
}

int set_char_conversion(word name, word value);
int set_debug(word name, word value);
int set_unknown(word name, word value);
int set_double_quotes(word name, word value);

map_t flags = NULL;



prolog_flag_t bounded_flag = {"bounded", static_flag};
prolog_flag_t max_integer_flag = {"max_integer", static_flag};
prolog_flag_t min_integer_flag = {"min_integer", static_flag};
prolog_flag_t integer_rounding_function_flag = {"integer_rounding_function", static_flag};
prolog_flag_t max_arity_flag = {"max_arity", static_flag};
prolog_flag_t char_conversion_flag = {"char_conversion", set_char_conversion};
prolog_flag_t debug_flag = {"debug", set_debug};
prolog_flag_t unknown_flag = {"unknown", set_unknown};
prolog_flag_t double_quotes_flag = {"double_quotes", set_double_quotes};



int set_char_conversion(word name, word value)
{
   if (value == offAtom)
      char_conversion_flag.value = offAtom;
   else if (value == onAtom)
      char_conversion_flag.value = onAtom;
   else
      return SET_EXCEPTION(domain_error(flagValueAtom, MAKE_VCOMPOUND(addFunctor, name, value)));
   return 1;
}


int set_debug(word name, word value)
{
   if (value == offAtom)
      debug_flag.value = offAtom;
   else if (value == onAtom)
      debug_flag.value = onAtom;
   else
      return SET_EXCEPTION(domain_error(flagValueAtom, MAKE_VCOMPOUND(addFunctor, name, value)));
   return 1;
}


int set_unknown(word name, word value)
{
   if (value == failAtom)
      unknown_flag.value = failAtom;
   else if (value == errorAtom)
      unknown_flag.value = errorAtom;
   else if (value == warningAtom)
      unknown_flag.value = warningAtom;
   else
      return SET_EXCEPTION(domain_error(flagValueAtom, MAKE_VCOMPOUND(addFunctor, name, value)));
   return 1;
}


int set_double_quotes(word name, word value)
{
   if (value == codesAtom)
      double_quotes_flag.value = codesAtom;
   else if (value == charsAtom)
      double_quotes_flag.value = charsAtom;
   else if (value == atomAtom)
      double_quotes_flag.value = atomAtom;
   else
      return SET_EXCEPTION(domain_error(flagValueAtom, MAKE_VCOMPOUND(addFunctor, name, value)));
   return 1;
}



word get_prolog_flag(char* name)
{
   prolog_flag_t* w;
   if (hashmap_get(flags, name, (any_t*)&w) == MAP_OK)
      return w->value;
   return 0;
}

int set_prolog_flag(word flag, word value)
{
   prolog_flag_t* _flag;
   if (hashmap_get(flags, getConstant(flag).data.atom_data->data, (any_t*)&_flag) == MAP_OK)
      return _flag->set(flag, value);
   return SET_EXCEPTION(domain_error(prologFlagAtom, flag));
}


void initialize_prolog_flags()
{
   flags = hashmap_new();
   bounded_flag.value = falseAtom;
   hashmap_put(flags, "bounded", &bounded_flag);
//   hashmap_put(flags, maxIntegerAtom, &max_integer_flag);
//   hashmap_put(flags, minIntegerAtom, &min_integer_flag);
   hashmap_put(flags, "integer_rounding_function", &integer_rounding_function_flag);
   integer_rounding_function_flag.value = towardZeroAtom;
//   hashmap_put(flags, maxArityAtom, &max_arity_flag);
   char_conversion_flag.value = offAtom;
   hashmap_put(flags, "char_conversion", &char_conversion_flag);
   debug_flag.value = offAtom;
   hashmap_put(flags, "debug", &debug_flag);
   unknown_flag.value = errorAtom;
   hashmap_put(flags, "unknown", &unknown_flag);
   double_quotes_flag.value = codesAtom;
   hashmap_put(flags, "double_quotes", &double_quotes_flag);
}

int build_prolog_flag_key_list(void* list, char* name, void* value)
{
   list_append((List*)list, MAKE_ATOM(name));
   return MAP_OK;
}

word prolog_flag_keys()
{
   List list;
   init_list(&list);
   hashmap_iterate(flags, build_prolog_flag_key_list, &list);
   word w = term_from_list(&list, emptyListAtom);
   free_list(&list);
   return w;
}
