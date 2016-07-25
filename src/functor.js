"use strict";
exports=module.exports;

var AtomTerm = require('./atom_term.js');
var CTable = require('./ctable.js');

var index = 0;

function Functor(name, arity)
{
    if (typeof name !== 'number')
        throw new Error("Bad functor:" + name);
    if (TAGOF(name) != ConstantTag)
        throw new Error("Bad functor - not constant:" + name);
    this.name = name;
    this.arity = arity;
}

Functor.prototype.toString = function()
{
    return CTable.get(this.name).value + "/" + this.arity
}

Functor.prototype.getClass = function()
{
    return "Functor";
}

Functor.prototype.hashCode = function()
{
    return this.toString();
}

Functor.prototype.equals = function(o)
{
    return o === this || (o instanceof Functor && o.name == this.name && o.arity == this.arity);
}

Functor.get = function(name, arity)
{
    return CTable.intern(new Functor(DEREF(name), arity));
}

module.exports = Functor;
