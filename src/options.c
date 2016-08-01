#include "options.h"
#include "constants.h"
#include "errors.h"
#include "ctable.h"
#include "whashmap.h"
#include "kernel.h"

void init_options(Options* o)
{
   o->map = whashmap_new();
}
void free_options(Options* o)
{
   whashmap_free(o->map);
}

void set_option(Options* o, word key, word value)
{
   whashmap_put(o->map, key, (any_t)value);
}

int options_from_term(Options* o, word t)
{
   while(TAGOF(t) == COMPOUND_TAG && FUNCTOROF(t) == listFunctor)
   {
      word head = ARGOF(t, 0);
      if (TAGOF(head) != COMPOUND_TAG)
         return type_error(optionAtom, head);
      Functor f = getConstant(FUNCTOROF(head)).data.functor_data;
      if (f->arity > 2)
         return type_error(optionAtom, head);
      if (f->arity == 1)
         set_option(o, f->name, trueAtom);
      else
         set_option(o, f->name, ARGOF(t, 0));
      t = ARGOF(t, 1);
   }
   return t == emptyListAtom;
}

word get_option(Options* o, word key, word defaultvalue)
{
   word w;
   if (whashmap_get(o->map, key, (any_t*)&w) == MAP_OK)
      return w;
   return defaultvalue;
}
