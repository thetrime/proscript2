#ifndef _ARITHMETIC_H
#define _ARITHMETIC_H
typedef enum
{
   IntegerType = 0,
   BigIntegerType = 1,
   FloatType = 2,
   RationalType = 3
} NumberType;


typedef struct
{
   NumberType type;
   union
   {
      long i;
      double f;
      mpz_t ii; // big integer
      mpq_t r; // rational
   };
} number;

double toFloatAndFree(number n);
int arith_compare(word a, word b);
int evaluate_term(word expr, word* result);
int evaluate(word a, number* n);


#endif
