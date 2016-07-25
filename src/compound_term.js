"use strict";
exports=module.exports;

var Functor = require('./functor.js');
var AtomTerm = require('./atom_term.js');
var Term = require('./term');
var CTable = require('./ctable');
var util = require('util');

function CompoundTerm(functor_name, args)
{
    if (typeof functor_name == 'string')
        this.functor = Functor.get(AtomTerm.get(functor_name), args.length);
    else
    {
        var fname = CTable.get(functor_name);
        if (fname instanceof AtomTerm)
        {
            this.functor = Functor.get(CTable.intern(fname), args.length);
        }
        else if (fname instanceof Functor)
            this.functor = functor_name;
        else
        {
            console.log(new Error().stack);
            throw new Error("Bad functor for compound:" + functor_name + " -> " + util.inspect(fname));
        }
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
    return CompoundTerm.create(this.functor, deref_args);
}

CompoundTerm.prototype.toString = function()
{
    var s = CTable.get(this.functor).name.toString() + "(";
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

CompoundTerm.create = function(functor_name, args)
{
    var ptr = HTOP | 0x80000000;
    var functor;
    if (typeof functor_name == 'string')
    {
        functor = Functor.get(AtomTerm.get(functor_name), args.length);
    }
    else
    {
        var fname = CTable.get(functor_name);
        if (fname instanceof AtomTerm)
        {
            functor = Functor.get(functor_name, args.length);
        }
        else if (fname instanceof Functor)
        {
            functor = functor_name;
        }
        else
        {
            console.log(new Error().stack);
            throw new Error("Bad functor for compound:" + functor_name + " -> " + util.inspect(fname));
        }
    }
    HEAP[HTOP++] = functor;
    for (var i = 0; i < args.length; i++)
    {
        if (args[i] == undefined)
            throw new Error("Undefined arg in compound");
        HEAP[HTOP++] = args[i] & 0xffffffff;
    }
    return (ptr & 0xffffffff) | 0;
}

module.exports = CompoundTerm;
