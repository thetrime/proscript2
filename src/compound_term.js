var Functor = require('./functor.js');
var AtomTerm = require('./atom_term.js');

function CompoundTerm(functor_name, args)
{
    if (functor_name instanceof AtomTerm)
	this.functor = Functor.get(functor_name, args.length);
    else if (typeof functor_name == 'string')
	this.functor = Functor.get(AtomTerm.get(functor_name), args.length);
    else if (functor_name instanceof Functor)
	this.functor = functor_name
    else
    {
	console.log(new Error().stack);
	throw "Bad input";
    }
    this.args = args;
}

CompoundTerm.prototype.dereference = function()
{
    return this;
}


module.exports = CompoundTerm;
