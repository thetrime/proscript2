util = require("util");

var nextvar = 0;

function VariableTerm(name)
{
    if (name === undefined)
        this.name = "G" + nextvar++;
    else
        this.name = name;
    this.value = null;
}

VariableTerm.prototype.dereference = function()
{
    var deref = this;
    while (true)
    {
	var value = deref.value;
	if (value == null)
	    return deref;
	if (value instanceof VariableTerm)
	    deref = value;
	else
	    return value;
    }
}

VariableTerm.prototype.toString = function()
{
    return "_" + this.name;
}

VariableTerm.prototype.equals = function(o)
{
    return (o === this) || (o instanceof VariableTerm && this.dereference().equals(o.dereference()));
}


module.exports = VariableTerm;
