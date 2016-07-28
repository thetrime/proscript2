#include <stdio.h>
#include "kernel.h"
#include "constants.h"
#include "stream.h"
#include "parser.h"
#include <string.h>

int main()
{
   initialize_constants();
   char* code = "fox(a,b):- \"hello\".";
   Stream s = stringBufferStream(strdup(code), strlen(code));
   word w = readTerm(s, NULL);
   PORTRAY(w); printf("\n");
   return -1;
}
