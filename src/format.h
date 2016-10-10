#ifndef _FORMAT_H
#define _FORMAT_H
#include "types.h"
#include "checks.h"
#include "ctable.h"
#include "string_builder.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "arithmetic.h"
#include "term_writer.h"
#include "stream.h"
#include "foreign.h"


#define NEXT_FORMAT_ARG {if(TAGOF(args) == COMPOUND_TAG && FUNCTOROF(args) == listFunctor) { arg = ARGOF(args, 0); args = ARGOF(args, 1); } else { freeStringBuilder(output); return format_error(MAKE_ATOM("Not enough arguments")); }}
#define BAD_FORMAT { freeStringBuilder(output); return ERROR; }

int format(word sink, word fmt, word args);

#endif
