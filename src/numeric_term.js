var BigInteger = require('big-integer');
var Rational = require('./rational');
var IntegerTerm = require('./integer_term');
var BigIntegerTerm = require('./biginteger_term');
var RationalTerm = require('./rational_term');

module.exports.get = function(t)
{
    if (t instanceof BigInteger)
    {
        if (t.isSmall)
            return new IntegerTerm(t.valueOf());
        return new BigIntegerTerm(t);
    }
    else if (t instanceof Rational)
    {
        return new RationalTerm(t);
    }
    else if (typeof t == "string")
    {
        if (t.length < 16)
            return new IntegerTerm(parseInt(t));
        var v = new BigInteger(t);
        if (v.isSmall)
            return new IntegerTerm(v.valueOf());
        return new BigIntegerTerm(v);
    }
    else if (typeof t == 'number')
        return new IntegerTerm(t);
    throw new Error("Bad numeric token: " + t);
}
