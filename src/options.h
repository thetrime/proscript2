#ifndef _OPTIONS_H
#define _OPTIONS_H
#include "types.h"
#include "whashmap.h"

struct options_t
{
   wmap_t map;
};

typedef struct options_t Options;

void init_options(Options* options);
void free_options(Options* options);
int options_from_term(Options* options, word term);
void set_option(Options*, word key, word value);
#endif
