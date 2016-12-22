#include "global.h"
#include <stdio.h>
#include "init.h"
#include "test.h"


int main(int argc, char** argv)
{
   init_prolog();
   do_test(argc, argv);
   return 0;
}

