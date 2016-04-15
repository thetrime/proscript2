var AtomTerm = require('./atom_term.js');

var functor_table = {};

function Functor(name, arity)
{
    this.name = name;
    this.arity = arity;
}

// name is an AtomTerm
Functor.get = function(name, arity)
{
    if (name instanceof AtomTerm)
    {
	if (functor_table[name.value] === undefined)
	    functor_table[name.value] = {};
	var arities = functor_table[name.value];
	if (arities[arity] === undefined)
	    arities[arity] = new Functor(name.value, arity);
	return arities[arity];
    }
    else
	throw("bad input to functor.get");
}


module.exports = Functor;
