#include "global.h"
#include <stdio.h>
#include "kernel.h"
#include "constants.h"
#include "stream.h"
#include "compiler.h"
#include "parser.h"
#include "ctable.h"
#include <string.h>
#include <unistd.h>
#ifdef EMSCRIPTEN
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

Choicepoint qxz = NULL;

void query_complete3(RC result)
{
   if (result == ERROR)
   {
      printf("Error reached the top-level: "); PORTRAY(getException()); printf("\n");
   }
   else if (result == FAIL)
   {
      printf("No more solutions\n");
   }
   else if (result == SUCCESS_WITH_CHOICES)
   {
      //printf("Success! %d\n", result);
      backtrack_query(query_complete3);
   }
}

void query_complete2(RC result)
{
   restore_state(qxz);
   backtrack_query(query_complete3);
}


void query_complete(RC result)
{
   //printf("First goal has exited with %d\n", result);
   if (result == ERROR)
   {
      printf("Error reached the top-level: "); PORTRAY(getException()); printf("\n");
   }
   else if (result == FAIL)
   {
      printf("Top level Failed\n");
   }
   else if (result == SUCCESS_WITH_CHOICES)
   {
      //printf("Success! %d\n", result);
      //qxz = push_state();
      //word w = MAKE_ATOM("run_all_tests");
      //execute_query(w, query_complete2);
      backtrack_query(query_complete3);
   }
   else if (result == SUCCESS)
      printf("Success!\n");
}



EMSCRIPTEN_KEEPALIVE
void do_test(int argc, char** argv)
{
   printf("Consulting...\n");
   int do_inria = 0;
   int do_yield = 0;
   for (int i = 0; i < argc; i++)
   {
      if (strcmp(argv[i], "--inria") == 0)
         do_inria = 1;
      if (strcmp(argv[i], "--yield") == 0)
         do_yield = 1;
   }
   if (do_inria)
   {
      consult_file("tests/inriasuite/inriasuite.pl");
      chdir("tests/inriasuite");
   }
   else
   {
      if (consult_file("test.pl"))
         printf("Consulted test.pl\n");
      else
         printf("Failed to load test.pl\n");
   }
   printf("Consulted. Running tests...\n");
   if (do_yield)
   {
      word ptr1 = MAKE_VAR();
      word goal1 = MAKE_VCOMPOUND(MAKE_FUNCTOR(MAKE_ATOM("yield_test"), 2), MAKE_ATOM("first_goal"), ptr1);
      execute_query(goal1, query_complete);
      printf("execute_query has returned\n");
      ExecutionCallback cb1 = (ExecutionCallback)getConstant(DEREF(ptr1), NULL).integer_data;
      printf("yield pointer is %p\n", cb1);

      // Now start a second goal that also yields
      Choicepoint saved = push_state();

      word ptr2 = MAKE_VAR();
      word goal2 = MAKE_VCOMPOUND(MAKE_FUNCTOR(MAKE_ATOM("yield_test"), 2), MAKE_ATOM("second_goal"), ptr2);
      execute_query(goal2, query_complete);
      printf("execute_query has returned\n");
      ExecutionCallback cb2 = (ExecutionCallback)getConstant(DEREF(ptr1), NULL).integer_data;
      printf("yield pointer is %p\n", cb2);
      resume_yield(SUCCESS, cb2);

      restore_state(saved);

      resume_yield(SUCCESS, cb1);
   }
   else
   {
      word w = MAKE_ATOM("run_all_tests");
      execute_query(w, query_complete);
   }
}

