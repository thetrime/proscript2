var AtomTerm = require('./atom_term.js');
var functor_map = {};
var functor_table = [];
var functor_index = 0;

function Functor(name, arity)
{
    this.name = name;
    this.arity = arity;
    functor_table[functor_index] = this;
    this.index = functor_index;
    functor_index++;
}

// name is an AtomTerm
Functor.get = function(name, arity)
{
    if (name instanceof AtomTerm)
    {
	if (functor_map[name.value] === undefined)
	    functor_map[name.value] = {};
	var arities = functor_map[name.value];
	if (arities[arity] === undefined)
	    arities[arity] = new Functor(name.value, arity);
	return arities[arity];
    }
    else
	throw("bad input to functor.get");
}

Functor.lookup = function(i)
{
    return functor_table[i];
}

module.exports = Functor;
