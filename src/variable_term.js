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
	if (value == null)
	    return deref;
	if (value instanceof VariableTerm)
	    deref = value;
    }
}


module.exports = VariableTerm;
