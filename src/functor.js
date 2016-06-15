var AtomTerm = require('./atom_term.js');

var index = 0;

function Functor(name, arity)
{
    if (!(name instanceof AtomTerm))
	throw("Functor() called with something not an atom: " + name);
    this.name = name;
    this.arity = arity;
}

Functor.prototype.toString = function()
{
    return this.name + "/" + this.arity
}

Functor.prototype.equals = function(o)
{
    return o === this || (o instanceof Functor && o.name.equals(this.name) && o.arity == this.arity);
}


module.exports = Functor;
