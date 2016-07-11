"use strict";

var Constants = require('./constants.js');
var CompoundTerm = require('./compound_term.js');
var AtomTerm = require('./atom_term.js');
var VariableTerm = require('./variable_term.js');
var IntegerTerm = require('./integer_term.js');
var FloatTerm = require('./float_term.js');
var BigIntegerTerm = require('./biginteger_term.js');
var BigInteger = require('big-integer');
var NumericTerm = require('./numeric_term');
var Rational = require('./rational.js');
var RationalTerm = require('./rational_term.js');
var Errors = require('./errors.js');
var Term = require('./term.js');
var util = require('util');


var strict_iso = false;
var is_unbounded = true;

function evaluate_args(args)
{
    var evaluated = new Array(args.length);
    for (var i = 0; i < args.length; i++)
    {
        evaluated[i] = evaluate_expression(args[i]);
    }
    return evaluated;
}

function toFloats(args)
{
    var floats = new Array(args.length);
    for (var i = 0; i < args.length; i++)
    {
        var v = evaluate_expression(args[i]);
        if (v instanceof IntegerTerm || v instanceof FloatTerm)
            floats[i] = Number(v.value);
        else if (v instanceof BigInteger || v instanceof RationalTerm)
            floats[i] = Number(v.value.valueOf());
        else
            throw new Error("Illegal float conversion from " + v.getClass());
    }
    return floats;
}

function toBigIntegers(args)
{
    var bi = new Array(args.length);
    for (var i = 0; i < args.length; i++)
    {
        var v = evaluate_expression(args[i]);
        if (v instanceof IntegerTerm)
            bi[i] = new BigInteger(v.value);
        else if (v instanceof BigIntegerTerm)
            bi[i] = v.value;
        else
            throw new Error("Illegal BigInteger conversion from " + v.getClass());
    }
    return bi;
}

function toRationals(args)
{
    var r = new Array(args.length);
    for (var i = 0; i < args.length; i++)
    {
        var v = evaluate_expression(args[i]);
        if (v instanceof RationalTerm)
            r[i] = v.value;
        else if (v instanceof BigIntegerTerm)
            r[i] = new Rational(v.value, BigInteger.one);
        else if (v instanceof IntegerTerm)
            r[i] = new Rational(new BigInteger(v.value), BigInteger.one);
        else
            throw new Error("Illegal BigInteger conversion from " + v.getClass());
    }
    return r;
}


function evaluate_expression(a)
{
    if ((a instanceof IntegerTerm) || (a instanceof FloatTerm) || (a instanceof BigIntegerTerm) || (a instanceof RationalTerm))
        return a;
    if (a instanceof VariableTerm)
        Errors.instantiationError(a);
    if (a instanceof CompoundTerm)
    {
        if (a.functor.equals(Constants.addFunctor))
        {
            var arg = evaluate_args(a.args);
            var target = commonType(arg[0], arg[1]);
            switch(target)
            {
                case "int":
                {
                    var result = arg[0] + arg[1];
                    if (result == ~~result)
                        return new IntegerTerm(result);
                    if (!is_unbounded)
                        Errors.integerOverflow();
                    // Fall-through and try again
                }
                case "bigint":
                {
                    var bi = toBigIntegers(arg);
                    return NumericTerm.get(bi[0].add(bi[1]));
                }
                case "float":
                {
                    var f = toFloats(arg);
                    var result = f[0] + f[1];
                    if ((result == Infinity) || (result == -Infinity))
                        Errors.floatOverflow();
                    return new FloatTerm(result);
                }
                case "rational":
                {
                    var r = toRationals(arg);
                    return NumericTerm.get(r[0].add(r[1]));
                }
            }
        }
        else if (a.functor.equals(Constants.subtractFunctor))
        {
            var arg = evaluate_args(a.args);
            var target = commonType(arg[0], arg[1]);
            switch(target)
            {
                case "int":
                {
                    var result = arg[0] - arg[1];
                    if (result == ~~result)
                        return new IntegerTerm(result);
                    if (!is_unbounded)
                        Errors.integerOverflow();
                    // Fall-through and try again
                }
                case "bigint":
                {
                    var bi = toBigIntegers(arg);
                    return NumericTerm.get(bi[0].subtract(bi[1]));
                }
                case "float":
                {
                    var f = toFloats(arg);
                    var result = f[0] - f[1];
                    if ((result == Infinity) || (result == -Infinity))
                        Errors.floatOverflow();
                    return new FloatTerm(result);
                }
                case "rational":
                {
                    var r = toRationals(arg);
                    return NumericTerm.get(r[0].subtract(r[1]));
                }
            }
        }
        else if (a.functor.equals(Constants.multiplyFunctor))
        {
            var arg = evaluate_args(a.args);
            var target = commonType(arg[0], arg[1]);
            switch(target)
            {
                case "int":
                {
                    var result = arg[0] * arg[1];
                    if (result == ~~result)
                        return new IntegerTerm(result);
                    if (!is_unbounded)
                        Errors.integerOverflow();
                    // Fall-through and try again
                }
                case "bigint":
                {
                    var bi = toBigIntegers(arg);
                    return NumericTerm.get(bi[0].multiply(bi[1]));
                }
                case "float":
                {
                    var f = toFloats(arg);
                    var result = f[0] * f[1];
                    if ((result == Infinity) || (result == -Infinity))
                        Errors.floatOverflow();
                    return new FloatTerm(result);
                }
                case "rational":
                {
                    var r = toRationals(arg);
                    return NumericTerm.get(r[0].multiply(r[1]));
                }
            }
        }
        else if (a.functor.equals(Constants.intDivFunctor))
        {
            var arg = evaluate_args(a.args);
            if (!(arg[0] instanceof IntegerTerm || arg[0] instanceof BigIntegerTerm))
                Errors.typeError(Constants.integerAtom, arg[0]);
            if (!(arg[1] instanceof IntegerTerm || arg[1] instanceof BigIntegerTerm))
                Errors.typeError(Constants.integerAtom, arg[1]);
            if (arg[0] instanceof IntegerTerm && arg[1] instanceof IntegerTerm)
            {
                if (arg[1].value == 0)
                    Errors.zeroDivisor();
                return new IntegerTerm(Math.trunc(arg[0] / arg[1]));
            }
            else
            {
                var bi = toBigIntegers(arg);
                if (bi[1].value.isZero())
                    Errors.zeroDivisor();
                return new BigIntegerTerm(bi[0].divide(bi[1]));
            }
        }
        else if (a.functor.equals(Constants.divisionFunctor))
        {
            var arg = evaluate_args(a.args);
            if (arg[0] instanceof RationalTerm || arg[1] instanceof RationalTerm)
            {
                // Rational
                var r = toRationals(arg);
                var res = r[0].divide(r[1]);
                return NumericTerm.get(res);
            }
            else
            {
                // Just convert everything to floats
                var d = toFloats(arg);
                if (d[1] == 0)
                    Errors.zeroDivisor();
                var result = d[0] / d[1];
                if (result == Infinity || result == -Infinity)
                    Errors.floatOverflow();
                return new FloatTerm(result);
            }
        }
        else if (a.functor.equals(Constants.remainderFunctor))
        {
            var arg = evaluate_args(a.args);
            if (!(arg[0] instanceof IntegerTerm || arg[0] instanceof BigIntegerTerm))
                Errors.typeError(Constants.integerAtom, arg[0]);
            if (!(arg[1] instanceof IntegerTerm || arg[1] instanceof BigIntegerTerm))
                Errors.typeError(Constants.integerAtom, arg[1]);
            if (arg[0] instanceof IntegerTerm && arg[1] instanceof IntegerTerm)
            {
                if (arg[1].value == 0)
                    Errors.zeroDivisor();
                return new IntegerTerm(arg[0] % arg[1]);
            }
            else
            {
                var bi = toBigIntegers(arg);
                return NumericTerm.get(bi[0].remainder(bi[1]));
            }
        }
        else if (a.functor.equals(Constants.moduloFunctor))
        {
            var arg = evaluate_args(a.args);
            if (!(arg[0] instanceof IntegerTerm || arg[0] instanceof BigIntegerTerm))
                Errors.typeError(Constants.integerAtom, arg[0]);
            if (!(arg[1] instanceof IntegerTerm || arg[1] instanceof BigIntegerTerm))
                Errors.typeError(Constants.integerAtom, arg[1]);
            if (arg[0] instanceof IntegerTerm && arg[1] instanceof IntegerTerm)
            {
                if (arg[1].value == 0)
                    Errors.zeroDivisor();
                return new IntegerTerm(arg[0].value - Math.floor(arg[0].value / arg[1].value) * arg[1].value);
            }
            else
            {
                var bi = toBigIntegers(arg);
                return NumericTerm.get(bi[0].mod(bi[1]));
            }
        }
        else if (a.functor.equals(Constants.negateFunctor))
        {
            var arg = evaluate_args(a.args);
            if (arg[0] instanceof IntegerTerm)
            {
                if (arg[0] == Number.MIN_SAFE_INTEGER)
                {
                    if (isUnbounded)
                        return new BigIntegerTerm(new BigInteger(arg[0].value).negate())
                    Errors.integerOverflow();
                }
                return new IntegerTerm(-arg[0].value);
            }
            else if (arg[0] instanceof FloatTerm)
            {
                var result = -arg[0].value;
                if (result == Infinity || result == -Infinity)
                    Errors.floatOverflow();
                return new FloatTerm(result);
            }
            else if (arg[0] instanceof BigIntegerTerm)
            {
                return new BigIntegerTerm(arg[0].value.negate());
            }
            else if (arg[0] instanceof RationalTerm)
            {
                return new RationalTerm(arg[0].value.negate());
            }
        }
        else if (a.functor.equals(Constants.absFunctor))
        {
            var arg = evaluate_args(a.args);
            var t = arg[0];
            if ((t instanceof IntegerTerm) || (t instanceof FloatTerm))
            {
                return new IntegerTerm(Math.abs(t));
            }
            else if ((t instanceof BigIntegerTerm) || (t instanceof RationalTerm))
            {
                return NumericTerm.get(t.abs());
            }
        }
        else if (a.functor.equals(Constants.signFunctor))
        {
            var arg = evaluate_args(a.args);
            var t = arg[0];
            if ((t instanceof IntegerTerm) || (t instanceof FloatTerm))
            {
                if (t.value == 0)
                    return new IntegerTerm(0);
                else if (t.value > 0)
                    return new IntegerTerm(1);
                return new IntegerTerm(0);
            }
            else if ((t instanceof BigIntegerTerm) || (t instanceof RationalTerm))
            {
                return new IntegerTerm(t.signum());
            }
        }
        else if (a.functor.equals(Constants.floatIntegerPartFunctor))
        {
            var arg = evaluate_args(a.args);
            if (arg[0] instanceof IntegerTerm || arg[0] instanceof BigIntegerTerm)
            {
                if (strict_iso)
                    typeError(Constants.floatAtom, arg[0]);
                return arg[0];
            }
            else if (arg[0] instanceof FloatTerm)
            {
                var sign = arg[0].value >= 0 ? 1 : -1;
                return new FloatTerm(sign * Math.floor(Math.abs(arg[0].value)));
            }
            else if (arg[0] instanceof RationalTerm)
            {
                return new BigIntegerTerm(arg[0].value.numerator.divide(arg[0].value.denominator));
            }
        }
        else if (a.functor.equals(Constants.floatFractionPartFunctor))
        {
            var arg = evaluate_args(a.args);
            if (arg[0] instanceof IntegerTerm || arg[0] instanceof BigIntegerTerm)
            {
                if (strict_iso)
                    typeError(Constants.floatAtom, arg[0]);
                return new IntegerTerm(0);
            }
            else if (arg[0] instanceof FloatTerm)
            {
                var sign = arg[0].value >= 0 ? 1 : -1;
                return new FloatTerm(arg[0].value - (sign * Math.floor(Math.abs(arg[0].value))));
            }
            else if (arg[0] instanceof RationalTerm)
            {
                var quotient = arg[0].value.numerator.divide(arg[0].value.denominator);
                var r = new Rational(quotient, new BigInteger(1));
                return new RationalTerm(arg[0].value.subtract(r));
            }
        }
        else if (a.functor.equals(Constants.floatFunctor))
        {
            var arg = evaluate_args(a.args);
            if (arg[0] instanceof FloatTerm)
                return new arg[0];
            if (arg[0] instanceof IntegerTerm)
                return new FloatTerm(arg[0].value);
            else if (a instanceof BigIntegerTerm && !strict_iso)
                return arg[0].toFloat();
            else if (arg[0] instanceof RationalTerm && !strict_iso)
                return arg[0].toFloat();
            Errors.typeError(Constants.numberAtom, arg[0]);
        }
        else if (a.functor.equals(Constants.floorFunctor))
        {
            var arg = evaluate_args(a.args);
            if (arg[0] instanceof FloatTerm)
                return new IntegerTerm(Math.floor(arg[0].value));
            if (arg[0] instanceof IntegerTerm && !strict_iso)
                return arg[0];
            else if (a instanceof BigIntegerTerm && !strict_iso)
                return arg[0];
            else if (arg[0] instanceof RationalTerm && !strict_iso)
                return arg[0].truncate(); // FIXME: No?
            Errors.typeError(Constants.floatAtom, arg[0]);
        }
        else if (a.functor.equals(Constants.truncateFunctor))
        {
            var arg = evaluate_args(a.args);
            if (arg[0] instanceof FloatTerm)
                return new IntegerTerm(Math.trunc(arg[0].value));
            if (arg[0] instanceof IntegerTerm && !strict_iso)
                return arg[0];
            else if (a instanceof BigIntegerTerm && !strict_iso)
                return arg[0];
            else if (arg[0] instanceof RationalTerm && !strict_iso)
                return arg[0].truncate();
            Errors.typeError(Constants.floatAtom, arg[0]);
        }
        else if (a.functor.equals(Constants.roundFunctor))
        {
            var arg = evaluate_args(a.args);
            if (arg[0] instanceof FloatTerm)
                return new IntegerTerm(Math.round(arg[0].value));
            if (arg[0] instanceof IntegerTerm && !strict_iso)
                return arg[0];
            else if (a instanceof BigIntegerTerm && !strict_iso)
                return arg[0];
            else if (arg[0] instanceof RationalTerm && !strict_iso)
                return arg[0].round();
            Errors.typeError(Constants.floatAtom, arg[0]);
        }
        else if (a.functor.equals(Constants.ceilingFunctor))
        {
            var arg  = evaluate_args(a.args);
            if (arg[0] instanceof FloatTerm)
                return new IntegerTerm(Math.ceil(arg[0].value));
            if (arg[0] instanceof IntegerTerm && !strict_iso)
                return arg[0];
            else if (a instanceof BigIntegerTerm && !strict_iso)
                return arg[0];
            else if (arg[0] instanceof RationalTerm && !strict_iso)
                return arg[0].ceiling();
            Errors.typeError(Constants.floatAtom, arg[0]);
        }
        // Can add in extensions here, otherwise we fall through to the evaluableError below
        else if (a.functor.equals(Constants.exponentiationFunctor))
        {
            var arg = evaluate_args(a.args);
            if (arg[0] instanceof FloatTerm || arg[1] instanceof FloatTerm)
            {
                var f = toFloats(arg);
                return new FloatTerm(Math.pow(f[0], f[1]));
            }
            else if (arg[0] instanceof IntegerTerm)
            {
                if (arg[1] instanceof IntegerTerm)
                {
                    var attempt = Math.pow(arg[0], arg[1]);
                    if (attempt == ~~attempt) // no loss of precision
                        return new IntegerTerm(attempt);
                    return new BigIntegerTerm(new BigInteger(arg[0].value).pow(arg[1]));
                }
                if (arg[1] instanceof BigIntegerTerm)
                    Errors.integerOverflow();
                if (arg[1] instanceof RationalTerm)
                {
                    throw new Error("Not implemented");
                }
            }
            else if (arg[0] instanceof BigIntegerTerm)
            {
                if (arg[1] instanceof IntegerTerm)
                {
                    throw new Error("Not implemented");
                }
                else if (arg[1] instanceof RationalTerm)
                {
                    throw new Error("Not implemented");
                }
                else if (arg[1] instanceof BigIntegerTerm)
                {
                    Errors.integerOverflow();
                }
            }
            else if (arg[0] instanceof RationalTerm)
            {
                throw new Error("Not implemented");
            }
        }
        else if (a.functor.equals(Constants.rdivFunctor) && !strict_iso)
        {
            var arg = evaluate_args(a.args);
            if (!(arg[0] instanceof IntegerTerm || arg[0] instanceof BigIntegerTerm))
                Errors.typeError(Constants.integerAtom, arg[0]);
            if (!(arg[1] instanceof IntegerTerm || arg[1] instanceof BigIntegerTerm))
                Errors.typeError(Constants.integerAtom, arg[1]);
            var bi = toBigIntegers(arg);
            return NumericTerm.get(Rational.rationalize(bi[0], bi[1]));
        }
    }
    //console.log("Could not evaluate: " + util.inspect(a));
    Errors.typeError(Constants.evaluableAtom, Term.predicate_indicator(a));
}

var type_names = ["int", "bigint", "float", "rational"];

function commonType(a, b)
{
    var type = 0;
    if (a instanceof BigIntegerTerm)
        type = 1;
    else if (a instanceof FloatTerm)
        type = 2;
    else if (a instanceof RationalTerm)
        type = 3;
    else if (!(a instanceof IntegerTerm))
        Errors.typeError(Constants.numericAtom, a);

    if (b instanceof BigIntegerTerm)
    {
        if (type < 1)
            type = 1;
    }
    else if (b instanceof FloatTerm)
    {
        if (type < 2)
            type = 2;
    }
    else if (b instanceof RationalTerm)
    {
        if (type < 3)
            type = 3;
    }
    else if (!(b instanceof IntegerTerm))
        Errors.typeError(Constants.numericAtom, b);
    return type_names[type];
}

function compare(a, b)
{
    var ae = evaluate_expression(a);
    var be = evaluate_expression(b);
    var targettype = commonType(ae, be);
    switch (targettype)
    {
        case "int":
        {
            if (ae.value > be.value)
                return 1;
            else if (ae.value == be.value)
                return 0;
            return -1;
        }
        case "bigint":
        {
            var bigints = toBigIntegers(ae, be);
            return bigints[0].compare(bigints[1].value);
        }
        case "float":
        {
            var floats = toFloats(ae, be);
            if (floats[0] == floats[1])
                return 0;
            else if (floats[0] > floats[1])
                return 1;
            return -1;
        }
        case "rational":
        {
            var rationals = toRationals(ae, be);
            return rationals[0].compare(rationals[1].value);
        }
    }
}

// 8.6.1
module.exports.is = function(result, expr)
{
    var q = evaluate_expression(expr);
    return this.unify(result, q);
}

// 8.7.1 Arithmetic comparison
module.exports["=\\="] = function(a, b)
{
    return compare(a, b) != 0;
}
module.exports["=:="] = function(a, b)
{
    return compare(a, b) == 0;
}
module.exports[">"] = function(a, b)
{
    return compare(a, b) > 0;
}
module.exports[">="] = function(a, b)
{
    return compare(a, b) >= 0;
}
module.exports["=<"] = function(a, b)
{
    return compare(a, b) <= 0;
}
module.exports["<"] = function(a, b)
{
    return compare(a, b) < 0;
}
