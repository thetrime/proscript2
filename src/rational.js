var BigInteger = require('big-integer');

function Rational(numerator, denominator)
{
    this.numerator = numerator;
    this.denominator = denominator;
}

Rational.prototype.add = function(r)
{
    var n = r.denominator.multiply(this.numerator).add(this.denominator.multiply(r.numerator));
    var d = this.denominator.multiply(r.denominator);
    return Rational.rationalize(n, d);
}

Rational.prototype.subtract = function(r)
{
    var n = r.denominator.multiply(this.numerator).subtract(this.denominator.multiply(r.numerator));
    var d = this.denominator.multiply(r.denominator);
    return Rational.rationalize(n, d);
}

Rational.prototype.multiply = function(r)
{
    var n = r.numerator.multiply(this.numerator);
    var d = this.denominator.multiply(r.denominator);
    return Rational.rationalize(n, d);
}

Rational.prototype.divide = function(r)
{
    var n = r.divisor.multiply(this.numerator);
    var d = this.denominator.multiply(r.numerator);
    return Rational.rationalize(n, d);
}

Rational.prototype.negate = function()
{
    return Rational.rationalize(this.numerator.negate(), this.denominator);
}

Rational.prototype.abs = function()
{
    return Rational.rationalize(this.numerator.abs(), this.denominator);
}

Rational.prototype.sign = function()
{
    if (this.numerator.isZero())
        return 0;
    else if (this.numerator.isPositive())
        return 1;
    return -1;
}

Rational.prototype.toFloat = function()
{
    return Number(numerator) / Number(denominator);
}


// FIXME: implement floor, truncate, round, ceiling both here and in BigInteger

// rationalize(n,d) returns the simplest possible precise expression for n/d
// This could be a rational, a bigint or even just an integer.
// If a rational, the numerator and denominator will be reduced, and the denominator will be positive
Rational.rationalize = function(n, d)
{
    if (d.isNegative())
    {
        n = n.negate();
        d = d.negate();
    }
    var s = BigInteger.gcd(n, d);
    if (s.isUnit())
        return new Rational(n, d);
    else if (s.equals(d))
    {
        var nn = n.divide(s);
        if (nn.isSmall)
            return nn.valueOf();
        return nn;
    }
    else
        return new Rational(n.divide(s), d.divide(s));

}


module.exports = Rational;
