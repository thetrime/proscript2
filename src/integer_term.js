function IntegerTerm(value)
{
    this.value = value;
}

IntegerTerm.prototype.dereference = function()
{
    return this;
}

IntegerTerm.prototype.equals = function(o)
{
    return (o === this) || ((o || {}).value === this.value);
}

module.exports = IntegerTerm;
