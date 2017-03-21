#include "global.h"
#include "types.h"
#include "ctable.h"
#include "kernel.h"
#include "errors.h"
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <math.h>
#include <gmp.h>
#include "arithmetic.h"

int is_unbounded = 1;

#ifndef STRICT_ISO
#define STRICT_ISO 0
#endif

int rand_init = 0;
gmp_randstate_t rand_state;


int arith_compare(word a, word b);

void free_number(number n)
{
   if (n.type == BigIntegerType)
   {
      mpz_clear(n.ii);
   }
   else if (n.type == RationalType)
   {
      mpq_clear(n.r);
   }
}


double toFloatAndFree(number n)
{
   switch(n.type)
   {
      case IntegerType:
         return (double)n.i;
      case FloatType:
         return (double)n.f;
      case BigIntegerType:
      {
         double d = mpz_get_d(n.ii);
         mpz_clear(n.ii);
         return d;
      }
      case RationalType:
      {
         double d = mpq_get_d(n.r);
         mpq_clear(n.r);
         return d;
      }
   }
}

void toBigIntegerAndFree(number n, mpz_t* m)
{
   mpz_init(*m);
   switch(n.type)
   {
      case IntegerType:
         mpz_set_si(*m, n.i); break;
      case BigIntegerType:
         mpz_set(*m, n.ii);
         mpz_clear(n.ii);
         break;
      case FloatType:
      case RationalType:
         assert(0 && "Illegal conversion to BigInteger");
   }
}

void toRationalAndFree(number n, mpq_t* m)
{
   mpq_init(*m);
   switch(n.type)
   {
      case IntegerType:
         mpq_set_si(*m, n.i, 1); break;
      case BigIntegerType:
      {
         mpz_t one;
         mpz_init(one);
         mpz_set_si(one, 1);
         mpq_set_num(*m, n.ii);
         mpq_set_den(*m, one);
         mpz_clear(one);
         mpz_clear(n.ii);
         break;
      }
      case FloatType:
         assert(0 && "Illegal conversion to Rational");
      case RationalType:
         mpq_set(*m, n.r);
         mpq_clear(n.r);
         break;
   }
}

int common_type(number a, number b)
{
   int type = 0;
   if (a.type == IntegerType)
      type = IntegerType;
   else if (a.type == BigIntegerType)
      type = BigIntegerType;
   else if (a.type == RationalType)
      type = RationalType;
   else if (a.type == FloatType)
      type = FloatType;
   else
      assert(0 && "Illegal number type");
   if (b.type == IntegerType)
   {
   }
   else if (b.type == BigIntegerType)
   {
      if (type < IntegerType)
         type = IntegerType;
   }
   else if (b.type == RationalType)
   {
      if (type < RationalType)
         type = RationalType;
   }
   else if (b.type == FloatType)
   {
      if (type < FloatType)
         type = FloatType;
   }
   else
      assert(0 && "Illegal number type");
   return type;
}

int evaluate(word a, number* n)
{
   if (TAGOF(a) == VARIABLE_TAG)
      return instantiation_error();
   else if (TAGOF(a) == CONSTANT_TAG)
   {
      int type;
      cdata ao = getConstant(a, &type);
      if (type == INTEGER_TYPE)
      {
         n->type = IntegerType;
         n->i = ao.integer_data;
         return 1;
      }
      else if (type == FLOAT_TYPE)
      {
         n->type = FloatType;
         n->f = ao.float_data->data;
         return 1;
      }
      else if (type == BIGINTEGER_TYPE)
      {
         n->type = BigIntegerType;
         mpz_init(n->ii);
         mpz_set(n->ii, ao.biginteger_data->data);
         return 1;
      }
      else if (type == RATIONAL_TYPE)
      {
         n->type = RationalType;
         mpq_init(n->r);
         mpq_set(n->r, ao.rational_data->data);
         return 1;
      }
      return type_error(evaluableAtom, predicate_indicator(a));
   }
   else if (TAGOF(a) == COMPOUND_TAG)
   {
      if (FUNCTOROF(a) == addFunctor)
      {
         number n0, n1;
         if (!(evaluate(ARGOF(a, 0), &n0) && evaluate(ARGOF(a, 1), &n1)))
            return 0;
         switch(common_type(n0, n1))
         {
            case IntegerType:
            {
               if (n0.i > 0 && n1.i > LONG_MAX - n0.i)
               {
                  // Overflow
                  if (!is_unbounded)
                     return integer_overflow();
                  // Otherwise drop through to the next case
               }
               else if (n0.i < 0 && n1.i < LONG_MIN - n0.i)
               {
                  // Underflow
                  if (!is_unbounded)
                     return integer_overflow();
                  // Otherwise drop through to the next case
               }
               else
               {
                  n->type = IntegerType;
                  n->i = n0.i + n1.i;
                  return 1;
               }
            }
            case BigIntegerType:
            {
               mpz_t b0, b1;
               toBigIntegerAndFree(n0, &b0);
               toBigIntegerAndFree(n1, &b1);
               n->type = BigIntegerType;
               mpz_init(n->ii);
               mpz_add(n->ii, b0, b1);
               mpz_clear(b0);
               mpz_clear(b1);
               return 1;
            }
            case FloatType:
            {
               double d0 = toFloatAndFree(n0);
               double d1 = toFloatAndFree(n1);
               n->f = d0+d1;
               n->type = FloatType;
               if (n->f == INFINITY || n->f == -INFINITY)
                  return float_overflow();
               return 1;
            }
            case RationalType:
            {
               mpq_t r0, r1;
               toRationalAndFree(n0, &r0);
               toRationalAndFree(n1, &r1);
               n->type = RationalType;
               mpq_init(n->r);
               mpq_add(n->r, r0, r1);
               mpq_clear(r0);
               mpq_clear(r1);
               return 1;
            }
         }
      }
      else if (FUNCTOROF(a) == subtractFunctor)
      {
         number n0, n1;
         if (!(evaluate(ARGOF(a, 0), &n0) && evaluate(ARGOF(a, 1), &n1)))
            return 0;
         switch(common_type(n0, n1))
         {
            case IntegerType:
            {
               if ((n1.i < 0) && (n0.i < LONG_MIN - n1.i))
               {
                  // Overflow
                  if (!is_unbounded)
                     return integer_overflow();
                  // Otherwise drop through to the next case
               }
               else if ((n1.i > 0) && (n0.i > LONG_MAX - n1.i))
               {
                  // Underflow
                  if (!is_unbounded)
                     return integer_overflow();
                  // Otherwise drop through to the next case
               }
               else
               {
                  n->type = IntegerType;
                  n->i = n0.i - n1.i;
                  return 1;
               }
            }
            case BigIntegerType:
            {
               mpz_t b0, b1;
               toBigIntegerAndFree(n0, &b0);
               toBigIntegerAndFree(n1, &b1);
               n->type = BigIntegerType;
               mpz_init(n->ii);
               mpz_sub(n->ii, b0, b1);
               mpz_clear(b0);
               mpz_clear(b1);
               return 1;
            }
            case FloatType:
            {
               double d0 = toFloatAndFree(n0);
               double d1 = toFloatAndFree(n1);
               n->type = FloatType;
               n->f = d0-d1;
               if (n->f == INFINITY || n->f == -INFINITY)
                  return float_overflow();
               return 1;
            }
            case RationalType:
            {
               mpq_t r0, r1;
               toRationalAndFree(n0, &r0);
               toRationalAndFree(n1, &r1);
               n->type = RationalType;
               mpq_init(n->r);
               mpq_sub(n->r, r0, r1);
               mpq_clear(r0);
               mpq_clear(r1);
               return 1;
            }
         }
      }
      else if (FUNCTOROF(a) == multiplyFunctor)
      {
         number n0, n1;
         if (!(evaluate(ARGOF(a, 0), &n0) && evaluate(ARGOF(a, 1), &n1)))
            return 0;
         switch(common_type(n0, n1))
         {
            case IntegerType:
            {
               if (__builtin_smull_overflow(n0.i, n1.i, &n->i))
               {
                  if (!is_unbounded)
                     return integer_overflow();
               }
               else
               {
                  n->type = IntegerType;
                  return 1;
               }
            }
            case BigIntegerType:
            {
               mpz_t b0, b1;
               toBigIntegerAndFree(n0, &b0);
               toBigIntegerAndFree(n1, &b1);
               n->type = BigIntegerType;
               mpz_init(n->ii);
               mpz_mul(n->ii, b0, b1);
               mpz_clear(b0);
               mpz_clear(b1);
               return 1;
            }
            case FloatType:
            {
               double d0 = toFloatAndFree(n0);
               double d1 = toFloatAndFree(n1);
               n->type = FloatType;
               n->f = d0*d1;
               if (n->f == INFINITY || n->f == -INFINITY)
                  return float_overflow();
               return 1;
            }
            case RationalType:
            {
               mpq_t r0, r1;
               toRationalAndFree(n0, &r0);
               toRationalAndFree(n1, &r1);
               n->type = RationalType;
               mpq_init(n->r);
               mpq_mul(n->r, r0, r1);
               mpq_clear(r0);
               mpq_clear(r1);
               return 1;
            }
         }
      }
      else if (FUNCTOROF(a) == intDivFunctor)
      {
         number n0, n1;
         if (!(evaluate(ARGOF(a, 0), &n0) && evaluate(ARGOF(a, 1), &n1)))
            return 0;
         if (!(n0.type == IntegerType || n0.type == BigIntegerType))
         {
            free_number(n0);
            free_number(n1);
            return type_error(integerAtom, ARGOF(a,0));
         }
         if (!(n1.type == IntegerType || n1.type == BigIntegerType))
         {
            free_number(n0);
            free_number(n1);
            return type_error(integerAtom, ARGOF(a,1));
         }
         if (n0.type == IntegerType && n0.type == IntegerType)
         {
            if (n1.i == 0)
            {
               free_number(n0);
               free_number(n1);
               return zero_divisor();
            }
            n->type = IntegerType;
            n->i = n0.i / n1.i;
            return 1;
         }
         else
         {
            mpz_t b0, b1;
            toBigIntegerAndFree(n0, &b0);
            toBigIntegerAndFree(n1, &b1);
            if (mpz_cmp_ui(b1, 1) == 0)
            {
               mpz_clear(b0);
               mpz_clear(b1);
               return zero_divisor();
            }
            mpz_init(n->ii);
            mpz_tdiv_q(n->ii, b0, b1);
            if (mpz_fits_slong_p(n->ii))
            {
               long l = mpz_get_si(n->ii);
               mpz_clear(n->ii);
               n->type = IntegerType;
               n->i = l;
               return 1;
            }
            n->type = BigIntegerType;
            return 1;
         }
      }
      else if (FUNCTOROF(a) == divideFunctor)
      {
         number n0, n1;
         if (!(evaluate(ARGOF(a, 0), &n0) && evaluate(ARGOF(a, 1), &n1)))
            return 0;
         if (n0.type == RationalType && n1.type == RationalType)
         {
            mpq_t r0, r1;
            toRationalAndFree(n0, &r0);
            toRationalAndFree(n1, &r1);
            if (mpq_cmp_ui(r1, 0, 1) == 0)
            {
               mpq_clear(r0);
               mpq_clear(r1);
               return zero_divisor();
            }
            mpq_init(n->r);
            mpq_div(n->r, r0, r1);
            n->type = RationalType;
            return 1;
         }
         else
         {
            // Convert everything to floats
            double d0 = toFloatAndFree(n0);
            double d1 = toFloatAndFree(n1);
            if (d1 == 0)
               return zero_divisor();
            n->type = FloatType;
            n->f = d0/d1;
            return 1;
         }
      }
      else if (FUNCTOROF(a) == remainderFunctor)
      {
         number n0, n1;
         if (!(evaluate(ARGOF(a, 0), &n0) && evaluate(ARGOF(a, 1), &n1)))
            return 0;
         if (!(n0.type == IntegerType || n0.type == BigIntegerType))
         {
            free_number(n0);
            free_number(n1);
            return type_error(integerAtom, ARGOF(a,0));
         }
         if (!(n1.type == IntegerType || n1.type == BigIntegerType))
         {
            free_number(n0);
            free_number(n1);
            return type_error(integerAtom, ARGOF(a,1));
         }
         if (n0.type == IntegerType && n1.type == IntegerType)
         {
            if (n1.i == 0)
               return zero_divisor();
            n->type = IntegerType;
            n->i = n0.i % n1.i;
            return 1;
         }
         else
         {
            mpz_t b0, b1;
            toBigIntegerAndFree(n0, &b0);
            toBigIntegerAndFree(n1, &b1);
            if (mpz_cmp_ui(b1, 1) == 0)
            {
               mpz_clear(b0);
               mpz_clear(b1);
               return zero_divisor();
            }
            mpz_init(n->ii);
            mpz_tdiv_r(n->ii, b0, b1);
            if (mpz_fits_slong_p(n->ii))
            {
               long l = mpz_get_si(n->ii);
               mpz_clear(n->ii);
               n->type = IntegerType;
               n->i = l;
               return 1;
            }
            n->type = BigIntegerType;
            return 1;
         }
      }
      else if (FUNCTOROF(a) == moduloFunctor)
      {
         number n0, n1;
         if (!(evaluate(ARGOF(a, 0), &n0) && evaluate(ARGOF(a, 1), &n1)))
            return 0;
         if (!(n0.type == IntegerType || n0.type == BigIntegerType))
         {
            free_number(n0);
            free_number(n1);
            return type_error(integerAtom, ARGOF(a,0));
         }
         if (!(n1.type == IntegerType || n1.type == BigIntegerType))
         {
            free_number(n0);
            free_number(n1);
            return type_error(integerAtom, ARGOF(a,1));
         }
         if (n0.type == IntegerType && n1.type == IntegerType)
         {
            if (n1.i == 0)
               return zero_divisor();
            n->type = IntegerType;
            n->i = n0.i - (n0.i/n1.i) * n1.i;
            return 1;
         }
         else
         {
            mpz_t b0, b1;
            toBigIntegerAndFree(n0, &b0);
            toBigIntegerAndFree(n1, &b1);
            if (mpz_cmp_ui(b1, 1) == 0)
            {
               mpz_clear(b0);
               mpz_clear(b1);
               return zero_divisor();
            }
            mpz_init(n->ii);
            mpz_mod(n->ii, b0, b1);
            if (mpz_fits_slong_p(n->ii))
            {
               long l = mpz_get_si(n->ii);
               mpz_clear(n->ii);
               n->type = IntegerType;
               n->i = l;
               return 1;
            }
            n->type = BigIntegerType;
            return 1;
         }
      }
      else if (FUNCTOROF(a) == negateFunctor)
      {
         number n0;
         if (!(evaluate(ARGOF(a, 0), &n0)))
            return 0;
         if (n0.type == IntegerType)
         {
            if (n0.i == LONG_MIN)
            {
               if (is_unbounded)
               {
                  n->type = BigIntegerType;
                  mpz_t m;
                  mpz_init(m);
                  mpz_init(n->ii);
                  mpz_set_ui(m, n0.i);
                  mpz_neg(n->ii, m);
                  mpz_clear(m);
                  return 1;
               }
               else
                  return integer_overflow();
            }
            else
            {
               n->type = IntegerType;
               n->i = -n0.i;
               return 1;
            }
         }
         else if (n0.type == BigIntegerType)
         {
            n->type = BigIntegerType;
            mpz_init(n->ii);
            mpz_neg(n->ii, n0.ii);
            mpz_clear(n0.ii);
            return 1;
         }
         else if (n0.type == FloatType)
         {
            n->type = FloatType;
            n->f = -n0.f;
            return 1;
         }
         else if (n0.type == RationalType)
         {
            n->type = RationalType;
            mpq_init(n->r);
            mpq_neg(n->r, n0.r);
            mpq_clear(n0.r);
            return 1;
         }
      }
      else if (FUNCTOROF(a) == absFunctor)
      {
         number n0;
         if (!(evaluate(ARGOF(a, 0), &n0)))
            return 0;
         if (n0.type == IntegerType)
         {
            n->type = IntegerType;
            n->i = labs(n0.i);
            return 1;
         }
         else if (n0.type == BigIntegerType)
         {
            n->type = BigIntegerType;
            mpz_init(n->ii);
            mpz_abs(n->ii, n0.ii);
            mpz_clear(n0.ii);
            return 1;
         }
         else if (n0.type == FloatType)
         {
            n->type = FloatType;
            n->f = fabs(n0.f);
            return 1;
         }
         else if (n0.type == RationalType)
         {
            n->type = RationalType;
            mpq_init(n->r);
            mpq_abs(n->r, n0.r);
            mpq_clear(n0.r);
            return 1;
         }
      }
      else if (FUNCTOROF(a) == signFunctor)
      {
         number n0;
         if (!(evaluate(ARGOF(a, 0), &n0)))
            return 0;
         if (n0.type == IntegerType)
         {
            n->type = IntegerType;
            n->i = (0 < n0.i) - (n0.i < 0);
            return 1;
         }
         else if (n0.type == BigIntegerType)
         {
            n->type = IntegerType;
            n->i = mpz_sgn(n0.ii);
            mpz_clear(n0.ii);
            return 1;
         }
         else if (n0.type == FloatType)
         {
            n->type = FloatType;
            n->f = 0;
            if (n0.f > 0)
               n->f = 1;
            else if (n0.f < 0)
               n->f = -1;
            return 1;
         }
         else if (n0.type == RationalType)
         {
            n->type = IntegerType;
            n->i = mpq_sgn(n0.r);
            mpq_clear(n0.r);
            return 1;
         }
      }
      else if (FUNCTOROF(a) == floatIntegerPartFunctor)
      {
         number n0;
         if (!(evaluate(ARGOF(a, 0), &n0)))
            return 0;
         if (n0.type == IntegerType || n0.type == BigIntegerType)
         {
            if (STRICT_ISO)
            {
               free_number(n0);
               return type_error(floatAtom, ARGOF(a, 0));
            }
            if (n0.type == IntegerType)
            {
               n->type = IntegerType;
               n->i = n0.i;
               return 1;
            }
            if (n0.type == BigIntegerType)
            {
               n->type = BigIntegerType;
               mpz_init(n->ii);
               mpz_set(n->ii, n0.ii);
               mpz_clear(n0.ii);
               return 1;
            }
         }
         else if (n0.type == FloatType)
         {
            double d = toFloatAndFree(n0);
            int sign = d >= 0 ? 1 : -1;
            n->type = IntegerType;
            d = sign * floor(fabs(d));
            if ((d > LONG_MAX || d < LONG_MIN))
            {
               if (is_unbounded)
               {
                  n->type = BigIntegerType;
                  mpz_init(n->ii);
                  mpz_set_d(n->ii, d);
                  return 1;
               }
               return integer_overflow();
            }
            n->i = (long)d;
            return 1;
         }
         else if (n0.type == RationalType)
         {
            mpz_t num, den;
            mpz_init(n->ii);
            mpz_init(num);
            mpz_init(den);
            mpq_get_num(num, n0.r);
            mpq_get_num(den, n0.r);
            mpz_tdiv_q(n->ii, num, den);
            mpz_clear(den);
            mpz_clear(num);
            mpq_clear(n0.r);
            n->type = BigIntegerType;
            return 1;
         }
      }
      else if (FUNCTOROF(a) == floatFractionPartFunctor)
      {
         number n0;
         if (!(evaluate(ARGOF(a, 0), &n0)))
            return 0;
         if (n0.type == IntegerType || n0.type == BigIntegerType)
         {
            if (STRICT_ISO)
            {
               free_number(n0);
               return type_error(floatAtom, ARGOF(a, 0));
            }
            if (n0.type == IntegerType)
            {
               n->type = IntegerType;
               n->i = 0;
               return 1;
            }
            if (n0.type == BigIntegerType)
            {
               mpz_clear(n0.ii);
               n->type = IntegerType;
               n->i = 0;
               return 1;
            }
         }
         else if (n0.type == FloatType)
         {
            double d = toFloatAndFree(n0);
            int sign = d >= 0 ? 1 : -1;
            n->type = IntegerType;
            d = d - (sign * floor(fabs(d)));
            if ((d > LONG_MAX || d < LONG_MIN))
            {
               if (is_unbounded)
               {
                  n->type = BigIntegerType;
                  mpz_init(n->ii);
                  mpz_set_d(n->ii, d);
                  return 1;
               }
               return integer_overflow();
            }
            n->i = (long)d;
            return 1;
         }
         else if (n0.type == RationalType)
         {
            mpz_t num, den, q;
            mpq_t qq;
            mpq_init(n->r);
            mpz_init(num);
            mpz_init(den);
            mpz_init(q);
            mpq_init(qq);
            mpq_get_num(num, n0.r);
            mpq_get_num(den, n0.r);
            mpz_tdiv_q(q, num, den);
            mpq_set_num(qq, q);
            mpz_t one;
            mpz_init(one);
            mpz_set_ui(one, 1);
            mpq_set_den(qq, one);
            mpz_clear(one);
            mpz_clear(den);
            mpz_clear(num);
            mpq_sub(n->r, n0.r, qq);
            mpz_clear(q);
            mpq_clear(qq);
            mpq_clear(n0.r);
            n->type = RationalType;
            return 1;
         }
      }
      else if (FUNCTOROF(a) == floatFunctor)
      {
         number n0;
         if (!(evaluate(ARGOF(a, 0), &n0)))
            return 0;
         n->type = FloatType;
         if (n0.type == FloatType)
         {
            n->f = n0.f;
            return 1;
         }
         if (n0.type == IntegerType)
         {
            n->f = (double)n0.i;
            return 1;
         }
         if (n0.type == BigIntegerType)
         {
            n->f = mpz_get_d(n0.ii);
            mpz_clear(n0.ii);
            return 1;
         }
         if (n0.type == RationalType)
         {
            n->f = mpq_get_d(n0.r);
            mpq_clear(n0.r);
            return 1;
         }
      }
      else if (FUNCTOROF(a) == floorFunctor)
      {
         number n0;
         if (!(evaluate(ARGOF(a, 0), &n0)))
            return 0;
         if (n0.type == FloatType)
         {
            double d = floor(n0.f);
            n->type = IntegerType;
            if ((d > LONG_MAX || d < LONG_MIN))
            {
               if (is_unbounded)
               {
                  n->type = BigIntegerType;
                  mpz_init(n->ii);
                  mpz_set_d(n->ii, d);
                  return 1;
               }
               return integer_overflow();
            }
            n->i = (long)d;
            return 1;
         }
         if (n0.type == IntegerType)
         {
            if (STRICT_ISO)
               return type_error(floatAtom, ARGOF(a, 0));
            n->type = IntegerType;
            n->i = n0.i;
            return 1;
         }
         if (n0.type == BigIntegerType)
         {
            if (STRICT_ISO)
            {
               free_number(n0);
               return type_error(floatAtom, ARGOF(a, 0));
            }
            n->type = IntegerType;
            mpz_init(n->ii);
            mpz_set(n->ii, n0.ii);
            mpz_clear(n0.ii);
            return 1;
         }
         if (n0.type == RationalType)
         {
            mpz_t num, den;
            mpz_init(num);
            mpz_init(den);
            mpq_get_num(num, n0.r);
            mpq_get_den(den, n0.r);
            n->type = BigIntegerType;
            mpz_init(n->ii);
            mpz_fdiv_q(n->ii, num, den);
            mpz_clear(den);
            mpz_clear(num);
            mpq_clear(n0.r);
            if (mpz_fits_slong_p(n->ii))
            {
               long l = mpz_get_si(n->ii);
               mpz_clear(n->ii);
               n->type = IntegerType;
               n->i = l;
               return 1;
            }
            n->type = BigIntegerType;
            return 1;
         }
      }
      else if (FUNCTOROF(a) == truncateFunctor || FUNCTOROF(a) == roundFunctor)
      {
         number n0;
         if (!(evaluate(ARGOF(a, 0), &n0)))
            return 0;
         if (n0.type == FloatType)
         {
            double d = trunc(n0.f);
            n->type = IntegerType;
            if ((d > LONG_MAX || d < LONG_MIN))
            {
               if (is_unbounded)
               {
                  n->type = BigIntegerType;
                  mpz_init(n->ii);
                  mpz_set_d(n->ii, d);
                  return 1;
               }
               return integer_overflow();
            }
            n->i = (long)d;
            return 1;
         }
         if (n0.type == IntegerType)
         {
            if (STRICT_ISO)
               return type_error(floatAtom, ARGOF(a, 0));
            n->type = IntegerType;
            n->i = n0.i;
            return 1;
         }
         if (n0.type == BigIntegerType)
         {
            if (STRICT_ISO)
            {
               free_number(n0);
               return type_error(floatAtom, ARGOF(a, 0));
            }
            n->type = IntegerType;
            mpz_init(n->ii);
            mpz_set(n->ii, n0.ii);
            mpz_clear(n0.ii);
            return 1;
         }
         if (n0.type == RationalType)
         {
            mpz_t num, den;
            mpz_init(num);
            mpz_init(den);
            mpq_get_num(num, n0.r);
            mpq_get_den(den, n0.r);
            n->type = BigIntegerType;
            mpz_init(n->ii);
            mpz_tdiv_q(n->ii, num, den);
            mpz_clear(den);
            mpz_clear(num);
            mpq_clear(n0.r);
            if (mpz_fits_slong_p(n->ii))
            {
               long l = mpz_get_si(n->ii);
               mpz_clear(n->ii);
               n->type = IntegerType;
               n->i = l;
               return 1;
            }
            n->type = BigIntegerType;
            return 1;
         }
      }
      else if (FUNCTOROF(a) == ceilingFunctor)
      {
         number n0;
         if (!(evaluate(ARGOF(a, 0), &n0)))
            return 0;
         if (n0.type == FloatType)
         {
            double d = ceil(n0.f);
            n->type = IntegerType;
            if ((d > LONG_MAX || d < LONG_MIN))
            {
               if (is_unbounded)
               {
                  n->type = BigIntegerType;
                  mpz_init(n->ii);
                  mpz_set_d(n->ii, d);
                  return 1;
               }
               return integer_overflow();
            }
            n->i = (long)d;
            return 1;
         }
         if (n0.type == IntegerType)
         {
            if (STRICT_ISO)
               return type_error(floatAtom, ARGOF(a, 0));
            n->type = IntegerType;
            n->i = n0.i;
            return 1;
         }
         if (n0.type == BigIntegerType)
         {
            if (STRICT_ISO)
            {
               free_number(n0);
               return type_error(floatAtom, ARGOF(a, 0));
            }
            n->type = IntegerType;
            mpz_init(n->ii);
            mpz_set(n->ii, n0.ii);
            mpz_clear(n0.ii);
            return 1;
         }
         if (n0.type == RationalType)
         {
            mpz_t num, den;
            mpz_init(num);
            mpz_init(den);
            mpq_get_num(num, n0.r);
            mpq_get_den(den, n0.r);
            n->type = BigIntegerType;
            mpz_init(n->ii);
            mpz_cdiv_q(n->ii, num, den);
            mpz_clear(den);
            mpz_clear(num);
            mpq_clear(n0.r);
            if (mpz_fits_slong_p(n->ii))
            {
               long l = mpz_get_si(n->ii);
               mpz_clear(n->ii);
               n->type = IntegerType;
               n->i = l;
               return 1;
            }
            n->type = BigIntegerType;
            return 1;
         }
      }
      // Can add in extensions here, otherwise we fall through to the evaluableError below
      else if (FUNCTOROF(a) == exponentiationFunctor)
      {
         number n0, n1;
         if (!(evaluate(ARGOF(a, 0), &n0) && evaluate(ARGOF(a, 1), &n1)))
            return 0;
         if (n0.type == FloatType || n1.type == FloatType)
         {
            double d0 = toFloatAndFree(n0);
            double d1 = toFloatAndFree(n1);
            n->type = FloatType; // FIXME: if d0 is negative and d1 is not integer, then the result is actually complex
            n->f = pow(d0, d1);
            return 1;
         }
         else if (n0.type == IntegerType)
         {
            if (n1.type == IntegerType)
            {
               if (n1.i < 0 || n0.i < 0)
                  assert(0 && "Not implemented");
               mpz_init(n->ii);
               mpz_ui_pow_ui(n->ii, (unsigned long)n0.i, (unsigned long)n1.i);
               n->type = BigIntegerType;
               if (mpz_fits_slong_p(n->ii))
               {
                  long l = mpz_get_si(n->ii);
                  mpz_clear(n->ii);
                  n->type = IntegerType;
                  n->i = l;
                  return 1;
               }
               return 1;
            }
            else if (n1.type == BigIntegerType)
               return integer_overflow();
            else if (n1.type == RationalType)
            {
               assert(0 && "Not implemented");
            }
         }
         else if (n0.type == BigIntegerType)
         {
            if (n1.type == IntegerType)
            {
               mpz_init(n->ii);
               if (n1.i < 0)
                  assert(0 && "Not implemented");
               mpz_pow_ui(n->ii, n0.ii, (unsigned long)n1.i);
               n->type = BigIntegerType;
               if (mpz_fits_slong_p(n->ii))
               {
                  long l = mpz_get_si(n->ii);
                  mpz_clear(n->ii);
                  n->type = IntegerType;
                  n->i = l;
                  return 1;
               }
               return 1;
            }
            else if (n1.type == BigIntegerType)
               return integer_overflow();
            else if (n1.type == RationalType)
            {
               assert(0 && "Not implemented");
            }
         }
         else if (n0.type == RationalType)
         {
            if (n1.type == IntegerType)
            {
               assert(0 && "Not implemented");
            }
            else if (n1.type == BigIntegerType)
               return integer_overflow();
            else if (n1.type == RationalType)
            {
               assert(0 && "Not implemented");
            }
         }
      }
      else if (FUNCTOROF(a) == rdivFunctor && !STRICT_ISO)
      {
         number n0, n1;
         if (!(evaluate(ARGOF(a, 0), &n0) && evaluate(ARGOF(a, 1), &n1)))
            return 0;
         if (!(n0.type == IntegerType || n0.type == BigIntegerType))
         {
            free_number(n0);
            free_number(n1);
            return type_error(integerAtom, ARGOF(a,0));
         }
         if (!(n1.type == IntegerType || n1.type == BigIntegerType))
         {
            free_number(n0);
            free_number(n1);
            return type_error(integerAtom, ARGOF(a,1));
         }
         mpz_t b0, b1;
         toBigIntegerAndFree(n0, &b0);
         toBigIntegerAndFree(n1, &b1);
         n->type = RationalType;
         mpq_init(n->r);
         mpq_set_num(n->r, b0);
         mpq_set_den(n->r, b1);
         mpq_canonicalize(n->r);
         mpz_clear(b0);
         mpz_clear(b1);
         return 1;
      }
      else if (FUNCTOROF(a) == maxFunctor && !STRICT_ISO)
      {
         number na, nb;
         if (!(evaluate(ARGOF(a, 0), &na) && evaluate(ARGOF(a, 1), &nb)))
            return 0;
         int rc = arith_compare(ARGOF(a, 0), ARGOF(a, 1));
         if (rc == 1)
         {
            *n = na;
            free_number(nb);
         }
         else
         {
            *n = nb;
            free_number(na);
         }
         return 1;
      }
      else if (FUNCTOROF(a) == minFunctor && !STRICT_ISO)
      {
         number na, nb;
         if (!(evaluate(ARGOF(a, 0), &na) && evaluate(ARGOF(a, 1), &nb)))
            return 0;
         int rc = arith_compare(ARGOF(a, 0), ARGOF(a, 1));
         if (rc == -1)
         {
            *n = na;
            free_number(nb);
         }
         else
         {
            *n = nb;
            free_number(na);
         }
         return 1;
      }
      else if (FUNCTOROF(a) == randomFunctor && !STRICT_ISO)
      {
         number na;
         mpz_t ii;
         mpz_init(n->ii);
         if (!(evaluate(ARGOF(a, 0), &na)))
            return 0;
         // This is safe so long as we dont have multi-threading
         if (rand_init == 0)
         {
            gmp_randinit_mt(rand_state);
            rand_init = 1;
         }
         toBigIntegerAndFree(na, &ii);
         mpz_urandomm(n->ii, rand_state, ii);
         if (mpz_fits_slong_p(n->ii))
         {
            long l = mpz_get_si(n->ii);
            mpz_clear(n->ii);
            n->type = IntegerType;
            n->i = l;
            return 1;
         }
         n->type = BigIntegerType;
         return 1;
      }
   }
   return type_error(evaluableAtom, predicate_indicator(a));
}


int evaluate_term(word expr, word* result)
{
   int is_new;
   number n;
   if (!evaluate(expr, &n))
   {
      return 0;
   }
   word w;
   switch(n.type)
   {
      case IntegerType:
         w = MAKE_INTEGER(n.i); break;
      case FloatType:
         w = MAKE_FLOAT(n.f); break;
      case BigIntegerType:
         w = MAKE_BIGINTEGER(n.ii); break;
      case RationalType:
         w = MAKE_RATIONAL(n.r); break;
   }
   free_number(n);
   *result = w;
   return 1;
}

int arith_compare(word a, word b)
{
   number na, nb;
   if (evaluate(a, &na) && evaluate(b, &nb))
   {
      switch(common_type(na, nb))
      {
         case IntegerType:
         {
            if (na.i > nb.i)
               return 1;
            if (na.i == nb.i)
               return 0;
            return -1;
         }
         case BigIntegerType:
         {
            mpz_t ba, bb;
            toBigIntegerAndFree(na, &ba);
            toBigIntegerAndFree(nb, &bb);
            int rc = mpz_cmp(ba, bb);
            mpz_clear(ba);
            mpz_clear(bb);
            return rc;
         }
         case FloatType:
         {
            double fa, fb;
            fa = toFloatAndFree(na);
            fb = toFloatAndFree(nb);
            if (fa > fb)
               return 1;
            if (fa == fb)
               return 0;
            return -1;
         }
         case RationalType:
         {
            mpq_t ra, rb;
            toRationalAndFree(na, &ra);
            toRationalAndFree(nb, &rb);
            int rc = mpq_cmp(ra, rb);
            mpq_clear(ra);
            mpq_clear(rb);
            return rc;
         }
      }
   }
   return 0;
}
