#ifdef EMSCRIPTEN
#include <emscripten/emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

#include "constants.h"
#include "module.h"
#include "kernel.h"
#include "prolog_flag.h"

EMSCRIPTEN_KEEPALIVE
void init_prolog()
{
   initialize_constants();
   initialize_prolog_flags();
   initialize_modules();
   initialize_kernel();
}
