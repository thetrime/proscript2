"use strict";
exports=module.exports;

var CTable = require('./ctable');
var util = require('util');

window.HEAP = new Int32Array(655350); // FIXME: In progress. If we set this to Uint32Array then writing a number like (3 | 0xc0000000) to it and reading it back gives us a different value
window.HTOP = 1;
window.VariableTag = {};
window.ConstantTag = {};
window.CompoundTag = {};
window.UnknownTag = {};

window.DEREF = function(a)
{
    while (((a >>> 30) | 0) == 0)
    {
        if (a == 0 || a == HEAP[a]) return a;
        a = HEAP[a];
    }
    return a;
}


window.FUNCTOROF = function(a) // The functor is also, effectively, ARGOF(a, -1)
{
    return HEAP[DEREF(a) & 0x3fffffff];
}

window.ARGOF = function(a, i) // Return the ith arg of the compound a. Index is 0-based
{
    return DEREF(HEAP[(DEREF(a) & 0x3fffffff)+i+1]);
}

window.ARGPOF = function(a, i) // Return the address of the ith arg of the compound a. Index is 0-based
{
    return ((DEREF(a) & 0x3fffffff)+i+1) & 0xffffffff;
}


window.TAGOF = function(ref)
{
    var tag = (DEREF(ref) >>> 30) | 0;
    switch(tag)
    {
        case 0: return VariableTag;
        case 1: return UnknownTag;
        case 2: return CompoundTag;
        case 3: return ConstantTag;
    }
}

window.MAKEVAR = function() // Return a new unbound variable on the heap
{
    var index = HTOP;
    HEAP[index] = index;
    HTOP++;
    return index;
}

window.PORTRAY = function(ref)
{
    if (ref != (ref & 0xffffffff))
    {
        console.log(ref >>> 30);
        throw new Error("Bad ref ");
    }
    if (ref == 0)
    {
        // Unbound Variable
        return "_";
    }
    else if (TAGOF(ref) == VariableTag)
    {
        // Bound variable
        if (HEAP[ref] == ref)
            return "_G" + ref;
        return PORTRAY(HEAP[ref]);
    }
    else if (TAGOF(ref) == ConstantTag)
    {
        return CTable.get(DEREF(ref)).toString();
    }
    else if (TAGOF(ref) == CompoundTag)
    {
        var functor = CTable.get(FUNCTOROF(ref));
        var s = PORTRAY(functor.name) + "(";
        for (var i = 0; i < functor.arity; i++)
        {
            s = s + PORTRAY(ARGOF(ref, i));
            if (i+1 < functor.arity)
                s = s + ",";
        }
        return s + ")";
    }

}


module.exports = {Environment: require('./environment.js'),
                  AtomTerm: require('./atom_term.js'),
                  BlobTerm: require('./blob_term.js'),
                  Utils: require('./utils.js'),
                  VariableTerm: require('./variable_term.js'),
                  CompoundTerm: require('./compound_term.js'),
                  Constants: require('./constants.js'),
                  Opcodes: require('./opcodes.js').opcodes,
                  Errors: require('./errors.js'),
                  Parser: require('./parser.js'),
                  TermWriter: require('./term_writer.js'),
                  Functor: require('./functor.js')};
