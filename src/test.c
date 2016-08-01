#include <stdio.h>
#include "kernel.h"
#include "constants.h"
#include "stream.h"
#include "compiler.h"
#include "parser.h"
#include <string.h>
#ifdef EMSCRIPTEN
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif


//int main()
//{
//   initialize_constants();
//   return 0;
//}

EMSCRIPTEN_KEEPALIVE
void do_test()
{
   //consult_string("fox(a, b). fox(c, d):- badger(a). fox(c, x).");
   //consult_string("fox(a, b). fox(c, X/Y):- functor(boing(cat, dog), X, Y). fox(c, x).");
   consult_file("test.pl");

   word x = MAKE_ATOM("c");
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
      printf("Success! %d\n", result);
      PORTRAY(x); printf("\n");
      PORTRAY(y); printf("\n");
      while(result == SUCCESS_WITH_CHOICES)
      {
         printf("backtracking for other solutions...\n");
         result = backtrack_query();
         if (result == ERROR)
         {
            printf("Error: "); PORTRAY(getException()); printf("\n");
         }
         else if (result == FAIL)
            printf("Failed\n");
         printf("Success! %d\n", result);
         PORTRAY(x); printf("\n");
         PORTRAY(y); printf("\n");
      }
   }
}

/*
  fox(a,b).
  fox(c,d).

  ?-fox(c, X).
*/
