#include <stdio.h>
#include "kernel.h"
#include "constants.h"
#include "stream.h"
#include "compiler.h"
#include "parser.h"
#include <string.h>
#include <unistd.h>
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

   //consult_file("tests/inriasuite/inriasuite.pl"); chdir("tests/inriasuite");
   consult_file("test.pl");
   printf("Consulted. Running tests...\n");
   word w = MAKE_ATOM("run_all_tests");
   RC result = execute_query(w);
   if (result == ERROR)
   {
      printf("Error reached the top-level: "); PORTRAY(getException()); printf("\n");
   }
   else if (result == FAIL)
      printf("Top level Failed\n");
   else
   {
      printf("Success! %d\n", result);
      //PORTRAY(x); printf("\n");
      //PORTRAY(y); printf("\n");
      while(result == SUCCESS_WITH_CHOICES)
      {
         printf("backtracking for other solutions...\n");
         result = backtrack_query();
         if (result == ERROR)
         {
            printf("Error: "); PORTRAY(getException()); printf("\n");
         }
         else if (result == FAIL)
         {
            printf("No more solutions: Failed\n");
         }
         else
         {
            printf("Success! %d\n", result);
            //PORTRAY(x); printf("\n");
            //PORTRAY(y); printf("\n");
         }
      }
   }
}

/*
  fox(a,b).
  fox(c,d).

  ?-fox(c, X).
*/
