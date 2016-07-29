#include <stdio.h>
#include "kernel.h"
#include "constants.h"
#include "stream.h"
#include "compiler.h"
#include "parser.h"
#include <string.h>

void print_clause(Clause clause)
{
   for (int i = 0; i < clause->code_size; i++)
   {
      printf("@%d: %s ", i, instruction_info[clause->code[i]].name);
      if (instruction_info[clause->code[i]].flags & HAS_CONST)
      {
         int index = (clause->code[i+1] << 8) | (clause->code[i+2]);
         i+=2;
         PORTRAY(clause->constants[index]); printf(" ");
      }
      if (instruction_info[clause->code[i]].flags & HAS_ADDRESS)
      {
         long address = (clause->code[i+1] << 24) | (clause->code[i+2] << 16) | (clause->code[i+3] << 8) | (clause->code[i+4]);
         i+=4;
         printf("%lu ", address);
      }
      if (instruction_info[clause->code[i]].flags & HAS_SLOT)
      {
         int slot = (clause->code[i+1] << 8) | (clause->code[i+2]);
         i+=2;
         printf("%d ", slot);
      }
      printf("\n");
   }

}

int main()
{
   initialize_constants();
   char* code = "fox(X,a).";
   Stream s = stringBufferStream(strdup(code), strlen(code));
   word w = readTerm(s, NULL);
   execute_query(w);
   return -1;
}
