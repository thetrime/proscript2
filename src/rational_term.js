function RationalTerm(value)
{
    this.value = value;
}

RationalTerm.prototype.dereference = function()
{
    return this;
}

RationalTerm.prototype.equals = function(o)
{
    return (o === this) || ((o || {}).value === this.value);
}

RationalTerm.prototype.getClass = function()
{
    return "RationalTerm";
}

RationalTerm.prototype.toString = function()
{
    return this.value;
}

module.exports = RationalTerm;
