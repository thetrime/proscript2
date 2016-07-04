util = require("util");

var nextvar = 0;

function VariableTerm(name)
{
    if (name === undefined)
        this.name = "_G" + nextvar++;
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
    return this.name;
}

VariableTerm.prototype.equals = function(o)
{
    return (o === this ||   // Simple case - two things are equal if they are the same thing
            ((!(this.value == null && o.value == null) &&  // Only compare if we are not comparing two unbound vars
              (o instanceof VariableTerm && this.dereference().equals(o.dereference())))));
}


module.exports = VariableTerm;
