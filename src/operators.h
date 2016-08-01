#ifndef _OPERATORS_H
#define _OPERATORS_H
#include "types.h"

typedef enum
{
   FX, FY, XFX, XFY, YFX, XF, YF
} Fixity;

typedef struct
{
   int precedence;
   Fixity fixity;
   word functor;
} operator_t;

typedef operator_t* Operator;

struct op_cell_t
{
   struct op_cell_t* next;
   char* name;
   Operator op[3];
};

typedef struct op_cell_t op_cell_t;

typedef enum
{
   Prefix, Infix, Postfix
} OperatorPosition;

int find_operator(char* name, Operator* op, OperatorPosition position);
void add_operator(char* name, int precedence, Fixity fixity);
word make_op_list();
#endif
