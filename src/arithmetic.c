#include "types.h"
#include "ctable.h"
#include <assert.h>

int common_type(constant a, constant b)
{
   int type = 0;
   if (a.type == INTEGER_TYPE)
      type = 0;
   else if (a.type == BIGINTEGER_TYPE)
      type = 1;
   else if (a.type == FLOAT_TYPE)
      type = 2;
   else if (a.type == RATIONAL_TYPE)
      type = 3;
   else
      assert(0);
   if (b.type == BIGINTEGER_TYPE)
   {
      if (type < 1)
         type = 1;
   }
   else if (b.type == FLOAT_TYPE)
   {
      if (type < 2)
         type = 2;
   }
   else if (b.type == RATIONAL_TYPE)
   {
      if (type < 3)
         type = 3;
   }
   else if (b.type != INTEGER_TYPE)
      assert(0);
   return type;
}

constant evaluate(word expr)
{
   assert(0);
}

int arith_compare(word a, word b)
{
   constant ca = evaluate(a);
   constant cb = evaluate(b);
   switch(common_type(ca, cb))
   {
      case INTEGER_TYPE:
         if (ca.data.integer_data > cb.data.integer_data)
            return 1;
         if (ca.data.integer_data == cb.data.integer_data)
            return 0;
         return -1;
      default:
         assert(0);
   }
}
