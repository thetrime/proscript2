#include <stdio.h>
#include "kernel.h"
#include "constants.h"
#include "stream.h"
#include "compiler.h"
#include "parser.h"
#include <string.h>


int main()
{
   initialize_constants();

   consult_string("fox(a, b).");

   word x = MAKE_VAR();
   word y = MAKE_VAR();
   word w = MAKE_VCOMPOUND(MAKE_FUNCTOR(MAKE_ATOM("fox"), 2), x, y);
   RC result = execute_query(w);
   if (result == ERROR)
   {
      printf("Error: "); PORTRAY(getException()); printf("\n");
   }
   else if (result == FAIL)
      printf("Failed\n");
   else
   {
      printf("Success!\n");
      PORTRAY(x); printf("\n");
      PORTRAY(y); printf("\n");
   }

   return -1;
}
