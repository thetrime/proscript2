var Constants = require('./constants.js');
var CompoundTerm = require('./compound_term.js');
var AtomTerm = require('./atom_term.js');
var VariableTerm = require('./variable_term.js');
var IntegerTerm = require('./integer_term.js');
var FloatTerm = require('./float_term.js');
var BigIntegerTerm = require('./biginteger_term.js');
var RationalTerm = require('./rational_term.js');
var Errors = require('./errors.js');
var Term = require('./term.js');
var util = require('util');


var strict_iso = false;
var is_unbounded = true;

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
            Term.must_be_bound(a.args[0]);
            Term.must_be_bound(a.args[1]);
            var target = commonType(a.args[0], a.args[1]);
            switch(target)
            {
                case "int":
                {
                    var result = a.args[0] + a.args[1];
                    if (result == ~~result)
                        return new IntegerTerm(result);
                    if (!is_unbounded)
                        Errors.integerOverflow();
                    // Fall-through and try again
                }
                case "bigint":
                {
                    var bi = toBigIntegers(a.args);
                    return NumericTerm.get(bi[0].add(bi[1]));
                }
                case "float":
                {
                    var f = toFloats(a.args);
                    var result = f[0] + f[1];
                    if ((result == Infinity) || (result == -Infinity))
                        Errors.floatOverflow();
                    return new FloatTerm(result);
                }
                case "rational":
                {
                    var r = toRationals(a.args);
                    return NumericTerm.get(r[0].add(r[1]));
                }
            }
        }
        else if (a.functor.equals(Constants.subtractFunctor))
        {
            throw new Error("FIXME: Not implemented");
        }
        else if (a.functor.equals(Constants.multiplyFunctor))
        {
            throw new Error("FIXME: Not implemented");
        }
        else if (a.functor.equals(Constants.intDivFunctor))
        {
            throw new Error("FIXME: Not implemented");
        }
        else if (a.functor.equals(Constants.divisionFunctor))
        {
            throw new Error("FIXME: Not implemented");
        }
        else if (a.functor.equals(Constants.remainderFunctor))
        {
            Term.must_be_bound(a.args[0]);
            Term.must_be_bound(a.args[1]);
            if (!(a.args[0] instanceof IntegerTerm || a.args[0] instanceof BigIntegerTerm))
                Errors.typeError(Constants.integerAtom, a.args[0]);
            if (!(a.args[1] instanceof IntegerTerm || a.args[1] instanceof BigIntegerTerm))
                Errors.typeError(Constants.integerAtom, a.args[1]);
            if (a.args[0] instanceof IntegerTerm && a.args[1] instanceof IntegerTerm)
            {
                if (a.args[1].value == 0)
                    Errors.zeroDivisor();
                return new IntegerTerm(a.args[0] % a.args[1]);
            }
            else
            {
                var bi = toBigIntegers(a.args);
                return NumericTerm.get(bi[0].remainder(bi[1]));
            }
        }
        else if (a.functor.equals(Constants.moduloFunctor))
        {
            Term.must_be_bound(a.args[0]);
            Term.must_be_bound(a.args[1]);
            if (!(a.args[0] instanceof IntegerTerm || a.args[0] instanceof BigIntegerTerm))
                Errors.typeError(Constants.integerAtom, a.args[0]);
            if (!(a.args[1] instanceof IntegerTerm || a.args[1] instanceof BigIntegerTerm))
                Errors.typeError(Constants.integerAtom, a.args[1]);
            if (a.args[0] instanceof IntegerTerm && a.args[1] instanceof IntegerTerm)
            {
                if (a.args[1].value == 0)
                    Errors.zeroDivisor();
                return new IntegerTerm(a.args[0].value - Math.floor(a.args[0].value / a.args[1].value) * a.args[1].value);
            }
            else
            {
                var bi = toBigIntegers(a.args);
                return NumericTerm.get(bi[0].mod(bi[1]));
            }
        }
        else if (a.functor.equals(Constants.negateFunctor))
        {
            Term.must_be_bound(a.args[0]);
            if (a.args[0] instanceof IntegerTerm)
            {
                if (a.args[0] == Number.MIN_SAFE_INTEGER)
                {
                    if (isUnbounded)
                        return new BigIntegerTerm(new BigInteger(a.args[0].value).negate())
                    Errors.intOverflow();
                }
                return new IntegerTerm(-a.args[0].value);
            }
            else if (a.args[0] instanceof FloatTerm)
            {
                var result = -a.args[0].value;
                if (result == Infinity || result == -Infinity)
                    Errors.floatOverflow();
                return new FloatTerm(result);
            }
            else if (a.args[0] instanceof BigIntegerTerm)
            {
                return new BigIntegerTerm(a.args[0].value.negate());
            }
            else if (a.args[0] instanceof RationalTerm)
            {
                return new RationalTerm(a.args[0].value.negate());
            }
        }
        else if (a.functor.equals(Constants.absFunctor))
        {
            var t = a.args[0];
            Term.must_be_bound(t);
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
            var t = a.args[0];
            Term.must_be_bound(t);
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
            Term.must_be_bound(a.args[0]);
            if (a.args[0] instanceof IntegerTerm || a.args[0] instanceof BigIntegerTerm)
            {
                if (strict_iso)
                    typeError(Constants.floatAtom, a.args[0]);
                return a.args[0];
            }
            else if (a.args[0] instanceof FloatTerm)
            {
                var sign = a.args[0].value >= 0 ? 1 : -1;
                return new FloatTerm(sign * Math.floor(Math.abs(a.args[0].value)));
            }
            else if (a.args[0] instanceof RationalTerm)
            {
                return new BigIntegerTerm(a.args[0].value.numerator().divide(a.args[0].value.denominator()));
            }
        }
        else if (a.functor.equals(Constants.floatFractionPartFunctor))
        {
            Term.must_be_bound(a.args[0]);
            if (a.args[0] instanceof IntegerTerm || a.args[0] instanceof BigIntegerTerm)
            {
                if (strict_iso)
                    typeError(Constants.floatAtom, a.args[0]);
                return new IntegerTerm(0);
            }
            else if (a.args[0] instanceof FloatTerm)
            {
                var sign = a.args[0].value >= 0 ? 1 : -1;
                return new FloatTerm(a.args[0].value - (sign * Math.floor(Math.abs(a.args[0].value))));
            }
            else if (a.args[0] instanceof RationalTerm)
            {
                var quotient = a.args[0].value.numerator().divide(a.args[0].value.denominator());
                var r = new Rational(quotient, new BigInteger(1));
                return new RationalTerm(a.args[0].value.subtract(r));
            }
        }
        else if (a.functor.equals(Constants.floatFunctor))
        {
            Term.must_be_bound(a.args[0]);
            if (a.args[0] instanceof FloatTerm)
                return new a.args[0];
            if (a.args[0] instanceof IntegerTerm)
                return new FloatTerm(a.args[0].value);
            else if (a instanceof BigIntegerTerm && !strict_iso)
                return a.args[0].toFloat();
            else if (a.args[0] instanceof RationalTerm && !strict_iso)
                return a.args[0].toFloat();
            Errors.typeError(Constants.numberAtom, a.args[0]);
        }
        else if (a.functor.equals(Constants.floorFunctor))
        {
            Term.must_be_bound(a.args[0]);
            if (a.args[0] instanceof FloatTerm)
                return new IntegerTerm(Math.floor(a.args[0].value));
            if (a.args[0] instanceof IntegerTerm && !strict_iso)
                return a.args[0];
            else if (a instanceof BigIntegerTerm && !strict_iso)
                return a.args[0];
            else if (a.args[0] instanceof RationalTerm && !strict_iso)
                return a.args[0].truncate();
            Errors.typeError(Constants.floatAtom, a.args[0]);
        }
        else if (a.functor.equals(Constants.truncateFunctor))
        {
            Term.must_be_bound(a.args[0]);
            if (a.args[0] instanceof FloatTerm)
                return new IntegerTerm(Math.trunc(a.args[0].value));
            if (a.args[0] instanceof IntegerTerm && !strict_iso)
                return a.args[0];
            else if (a instanceof BigIntegerTerm && !strict_iso)
                return a.args[0];
            else if (a.args[0] instanceof RationalTerm && !strict_iso)
                return a.args[0].truncate();
            Errors.typeError(Constants.floatAtom, a.args[0]);
        }
        else if (a.functor.equals(Constants.roundFunctor))
        {
            Term.must_be_bound(a.args[0]);
            if (a.args[0] instanceof FloatTerm)
                return new IntegerTerm(Math.round(a.args[0].value));
            if (a.args[0] instanceof IntegerTerm && !strict_iso)
                return a.args[0];
            else if (a instanceof BigIntegerTerm && !strict_iso)
                return a.args[0];
            else if (a.args[0] instanceof RationalTerm && !strict_iso)
                return a.args[0].round();
            Errors.typeError(Constants.floatAtom, a.args[0]);
        }
        else if (a.functor.equals(Constants.ceilingFunctor))
        {
            Term.must_be_bound(a.args[0]);
            if (a.args[0] instanceof FloatTerm)
                return new IntegerTerm(Math.ceil(a.args[0].value));
            if (a.args[0] instanceof IntegerTerm && !strict_iso)
                return a.args[0];
            else if (a instanceof BigIntegerTerm && !strict_iso)
                return a.args[0];
            else if (a.args[0] instanceof RationalTerm && !strict_iso)
                return a.args[0].ceiling();
            Errors.typeError(Constants.floatAtom, a.args[0]);
        }
        // Can add in extensions here, otherwise we fall through to the evaluableError below
    }
    console.log("Could not evaluate: " + util.inspect(a));
    Errors.evaluableError(a);
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
            return 1;
        }
        case "bigint":
        {
            var bigints = toBigIntegers(ae, be);
            return bigints[0].compare(bigints[1].value);
        }
        case "float":
        {
            var floats = toFloats(ae, be);
            if (floats[0].value == floats[1].value)
                return 0;
            else if (floats[0].value > floats[1].value)
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
    return this.unify(result, evaluate_expression(expr));
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
