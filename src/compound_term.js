"use strict";
exports=module.exports;

var Functor = require('./functor.js');
var AtomTerm = require('./atom_term.js');
var Term = require('./term');

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
        throw new Error("Bad functor for compound:" + functor_name);
    }
    this.args = args;
}

CompoundTerm.prototype = new Term;

CompoundTerm.prototype.dereference_recursive = function()
{
    var deref_args = new Array(this.functor.arity);
    for (var i = 0; i < this.functor.arity; i++)
    {
        if (this.args[i] instanceof CompoundTerm)
            deref_args[i] = this.args[i].dereference_recursive();
        else
            deref_args[i] = this.args[i].dereference();
    }
    return new CompoundTerm(this.functor, deref_args);
}

CompoundTerm.prototype.toString = function()
{
    var s = this.functor.name.toString() + "(";
    for (var i = 0; i < this.args.length; i++)
    {
        if (this.args[i] === undefined)
            s += "<undefined>";
        else
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

CompoundTerm.prototype.getClass = function()
{
    return "CompoundTerm";
}


module.exports = CompoundTerm;
