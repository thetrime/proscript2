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

void query_complete(RC result)
{
   if (result == ERROR)
   {
      printf("Error reached the top-level: "); PORTRAY(getException()); printf("\n");
   }
   else if (result == FAIL)
   {
      printf("Top level Failed\n");
   }
   else
   {
      printf("Success! %d\n", result);
      if (result == SUCCESS_WITH_CHOICES)
      {
         printf("backtracking for other solutions...\n");
         backtrack_query(query_complete);
      }
   }
}


EMSCRIPTEN_KEEPALIVE
void do_test()
{

   consult_file("tests/inriasuite/inriasuite.pl"); chdir("tests/inriasuite");
   //consult_file("test.pl");
   printf("Consulted. Running tests...\n");
   word w = MAKE_ATOM("run_all_tests");
   execute_query(w, query_complete);
}

/*
  fox(a,b).
  fox(c,d).

  ?-fox(c, X).
*/
