util = require("util");

function VariableTerm(name)
{
    this.name = name;
    this.value = null;
}

VariableTerm.prototype.dereference = function()
{
    var deref = this;
    while (true)
    {
	var value = deref.value;
	console.log("value: " + util.inspect(value));
	if (value == null)
	    return deref;
	if (value instanceof VariableTerm)
	    deref = value;
	else
	    return value;
    }
}

VariableTerm.prototype.equals = function(o)
{
    return (o === this) || (o instanceof VariableTerm && this.dereference().equals(o.dereference()));
}


module.exports = VariableTerm;
