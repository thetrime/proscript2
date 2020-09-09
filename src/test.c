#include "global.h"
#include <stdio.h>
#include "kernel.h"
#include "constants.h"
#include "stream.h"
#include "compiler.h"
#include "parser.h"
#include "ctable.h"
#include "foreign.h"
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>
#ifdef EMSCRIPTEN
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

State qxz = NULL;

void query_complete3(RC result)
{
   if (result == ERROR)
   {
      printf("Error reached the top-level: "); PRETTY_PORTRAY(getException()); printf("\n");
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

time_t start;

void query_complete(RC result)
{
   //printf("First goal has exited with %d\n", result);
   if (result == ERROR)
   {
      printf("Error reached the top-level: "); PRETTY_PORTRAY(getException()); printf("\n");
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
      printf("Time taken for first solution: %lu milliseconds\n", (clock() - start) * 1000 / CLOCKS_PER_SEC);
      backtrack_query(query_complete3);
   }
   else if (result == SUCCESS)
      printf("Success!\n");
}


EMSCRIPTEN_KEEPALIVE
void do_test(int argc, char** argv)
{
   int do_inria = 0;
   int do_yield = 0;
   int do_agc = 0;
   int explicit_test = 0;
   for (int i = 1; i < argc; i++)
   {
      if (strcmp(argv[i], "--inria") == 0)
         do_inria = 1;
      else if (strcmp(argv[i], "--yield") == 0)
         do_yield = 1;
      else if (strcmp(argv[i], "--agc") == 0)
         do_agc = 1;
      else
      {
         printf("Consulting %s\n", argv[i]);
         if (consult_file(argv[i]))
            printf("Consulted %s\n", argv[i]);
         else
            printf("Failed to load %s\n", argv[i]);
         explicit_test = 1;
      }
   }
   if (do_inria)
   {
      consult_file("tests/inriasuite/inriasuite.pl");
      chdir("tests/inriasuite");
   }
   else if (do_agc)
   {
      consult_file("tests/agc.pl");
   }
   else if (!explicit_test)
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
      State saved = push_state();

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
   else if (do_agc)
   {
      int initial_atoms = get_constant_count();
      printf("Initial atom count: %d\n", initial_atoms);
      word w = acquire_constant("test", MAKE_ATOM("run_agc_test"));
      for (int i = 0; i < 100; i++)
      {
         State saved = push_state();
         start = clock();
         execute_query(w, query_complete);
         printf("Atoms after test %d: %d\n", i, get_constant_count());
         restore_state(saved);
         garbage_collect_constants();
         printf("Atoms after AGC: %d\n", get_constant_count());
      }
      printf("Initial atoms: %d, atoms now: %d\n", initial_atoms, get_constant_count());
   }
   else
   {
      word w = MAKE_ATOM("run_all_tests");
      int initial_atoms = get_constant_count();
      start = clock();
      execute_query(w, query_complete);
      printf("Atoms before AGC: %d\n", get_constant_count());
      garbage_collect_constants();
      printf("Atoms after AGC: %d\n", get_constant_count());
      printf("Initial atoms: %d\n", initial_atoms);
   }
}

