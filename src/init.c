#include "global.h"
#include <gmp.h>
#ifdef EMSCRIPTEN
#include <emscripten/emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include "constants.h"
#include "module.h"
#include "kernel.h"
#include "prolog_flag.h"
#include "record.h"
#include "char_conversion.h"

EMSCRIPTEN_KEEPALIVE
void init_prolog()
{
   mp_set_memory_functions(trace_malloc_gmp, trace_realloc_gmp, trace_free_gmp);
   initialize_constants();
   initialize_prolog_flags();
   initialize_database();
   initialize_modules();
   initialize_kernel();
   initialize_char_conversion();
}
