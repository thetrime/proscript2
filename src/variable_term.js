var Term = require('./term');

var nextvar = 0;

function VariableTerm(name)
{
    this.index = nextvar++;
    if (name === undefined)
        this.name = "_G" + this.index;
    else
        this.name = name;
    this.value = null;
}

VariableTerm.prototype = new Term;

VariableTerm.prototype.dereference_recursive = function()
{
    var deref = this.dereference();
    if (deref instanceof VariableTerm)
        return deref;
    return deref.dereference_recursive();
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

VariableTerm.prototype.getClass = function()
{
    return "VariableTerm";
}

VariableTerm.prototype.toString = function()
{
    if (this.value == null)
        return this.name;
    return this.dereference().toString();
}

VariableTerm.prototype.equals = function(o)
{
    return (o === this ||   // Simple case - two things are equal if they are the same thing
            (this.value != null && this.dereference().equals(o.dereference())));
}


module.exports = VariableTerm;
