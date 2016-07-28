#include "operators.h"
#include <string.h>
#include "kernel.h"

op_cell_t* operator_table = NULL;

void freeOperator(Operator op)
{
   free(op);
}

Operator allocOperator(word functor, int precedence, Fixity fixity)
{
   Operator op = malloc(sizeof(operator_t));
   op->functor = functor;
   op->precedence = precedence;
   op->fixity = fixity;
   return op;
}

void add_operator(char* name, int precedence, Fixity fixity)
{
   op_cell_t** cell = &operator_table;
   OperatorPosition position = Prefix;
   int arity = 0;
   switch(fixity)
   {
      case FX:
      case FY:
         position = Prefix; arity = 1; break;
      case XFX:
      case XFY:
      case YFX:
         position = Infix; arity = 2; break;
      case XF:
      case YF:
         position = Postfix; arity = 3; break;
   }
   while(*cell != NULL)
   {
      if (strcmp(name, (*cell)->name) == 0)
      {
         if ((*cell)->op[position] != NULL)
            freeOperator((*cell)->op[position]);
         (*cell)->op[position] = allocOperator(MAKE_FUNCTOR(MAKE_ATOM(name), arity), precedence, fixity);
         return;
      }
      cell = &((*cell)->next);
   }
   *cell = malloc(sizeof(op_cell_t));
   (*cell)->name = strdup(name);
   (*cell)->next = NULL;
   (*cell)->op[Prefix] = NULL;
   (*cell)->op[Infix] = NULL;
   (*cell)->op[Postfix] = NULL;
   (*cell)->op[position] = allocOperator(MAKE_FUNCTOR(MAKE_ATOM(name), arity), precedence, fixity);
}

void initialize_operators()
{
   add_operator(":-", 1200, FX);
   add_operator(":-", 1200, XFX);
   add_operator("?-", 1200, FX);
   add_operator("dynamic", 1150, FX);
   add_operator("discontiguous", 1150, FX);
   add_operator("initialization", 1150, FX);
   add_operator("meta_predicate", 1150, FX);
   add_operator("module_transparent", 1150, FX);
   add_operator("multifile", 1150, FX);
   add_operator("thread_local", 1150, FX);
   add_operator("volatile", 1150, FX);
   add_operator("\\+", 900, FY);
   add_operator("~", 900, FX);
   add_operator("?", 500, FX);
   add_operator("+", 200, FY);
   add_operator("+", 500, YFX);
   add_operator("-", 200, FY);
   add_operator("-", 500, YFX);
   add_operator("\\", 200, FY);
   add_operator("-->", 1200, XFX);
   add_operator(";", 1100, XFY);
   add_operator("|", 1100, XFY);
   add_operator("->", 1050, XFY);
   add_operator("*->", 1050, XFY);
   add_operator(",", 1000, XFY);
   add_operator(":=", 990, XFX);
   add_operator("<", 700, XFX);
   add_operator("=", 700, XFX);
   add_operator("=..", 700, XFX);
   add_operator("=@=", 700, XFX);
   add_operator("=:=", 700, XFX);
   add_operator("=<", 700, XFX);
   add_operator("==", 700, XFX);
   add_operator("=\\=", 700, XFX);
   add_operator(">", 700, XFX);
   add_operator(">=", 700, XFX);
   add_operator("@<", 700, XFX);
   add_operator("@=<", 700, XFX);
   add_operator("@>", 700, XFX);
   add_operator("@>=", 700, XFX);
   add_operator("\\=", 700, XFX);
   add_operator("\\==", 700, XFX);
   add_operator("is", 700, XFX);
   add_operator(">:<", 700, XFX);
   add_operator(":<", 700, XFX);
   add_operator(":", 600, XFY);
   add_operator("/\\", 500, YFX);
   add_operator("\\/", 500, YFX);
   add_operator("xor", 500, YFX);
   add_operator("*", 400, YFX);
   add_operator("/", 400, YFX);
   add_operator("//", 400, YFX);
   add_operator("rdiv", 400, YFX);
   add_operator("<<", 400, YFX);
   add_operator(">>", 400, YFX);
   add_operator("mod", 400, YFX);
   add_operator("rem", 400, YFX);
   add_operator("**", 200, XFX);
   add_operator("^", 200, XFY);
}


int find_operator(char* name, Operator* op, OperatorPosition position)
{
   if (operator_table == NULL)
      initialize_operators();
   op_cell_t* cell = operator_table;
   while(cell != NULL)
   {
      if (strcmp(name, cell->name) == 0)
      {
         *op = cell->op[position];
         return (*op != NULL);
      }
      cell = cell->next;
   }
   return 0;
}
