function AtomTerm(value)
{
    this.value = value;
}

AtomTerm.prototype.dereference = function()
{
    return this;
}

AtomTerm.prototype.toString = function()
{
    return this.value;
}

AtomTerm.prototype.equals = function(o)
{
    return (o === this) || ((o || {}).value === this.value);
}

AtomTerm.prototype.getClass = function()
{
    return "AtomTerm";
}


module.exports = AtomTerm;
