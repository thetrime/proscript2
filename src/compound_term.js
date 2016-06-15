var Functor = require('./functor.js');
var AtomTerm = require('./atom_term.js');

function CompoundTerm(functor_name, args)
{
    if (functor_name instanceof AtomTerm)
	this.functor = new Functor(functor_name, args.length);
    else if (typeof functor_name == 'string')
	this.functor = new Functor(new AtomTerm(functor_name), args.length);
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

CompoundTerm.prototype.toString = function()
{
    var s = this.functor.name.toString() + "(";
    for (var i = 0; i < this.args.length; i++)
    {
	s += this.args[i].dereference();
	if (i+1 < this.args.length)
	    s+=",";
    }
    return s + ")";
}

CompoundTerm.prototype.equals = function(o)
{
    // We should never be comparing compound terms directly!
    return false;
}

module.exports = CompoundTerm;
