function BigIntegerTerm(value)
{
    this.value = value;
}

BigIntegerTerm.prototype.dereference = function()
{
    return this;
}

BigIntegerTerm.prototype.equals = function(o)
{
    return (o === this) || ((o || {}).value === this.value);
}

BigIntegerTerm.prototype.getClass = function()
{
    return "BigIntegerTerm";
}

BigIntegerTerm.prototype.toString = function()
{
    return this.value;
}

module.exports = BigIntegerTerm;
