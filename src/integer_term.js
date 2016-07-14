var Term = require('./term');

function IntegerTerm(value)
{
    this.value = value;
}

IntegerTerm.prototype = new Term;

IntegerTerm.prototype.equals = function(o)
{
    return (o === this) || ((o || {}).value === this.value);
}

IntegerTerm.prototype.getClass = function()
{
    return "IntegerTerm";
}

IntegerTerm.prototype.toString = function()
{
    return this.value;
}

module.exports = IntegerTerm;
