function FloatTerm(value)
{
    this.value = value;
}

FloatTerm.prototype.dereference = function()
{
    return this;
}

FloatTerm.prototype.equals = function(o)
{
    return (o === this) || ((o || {}).value === this.value);
}

FloatTerm.prototype.toString = function()
{
    return this.value;
}

module.exports = FloatTerm;
