#ifndef _OPTIONS_H
#define _OPTIONS_H
#include "types.h"

struct options_t
{

};

typedef struct options_t Options;

void init_options(Options* options);
void free_options(Options* options);
int options_from_term(Options* options, word term);
#endif
