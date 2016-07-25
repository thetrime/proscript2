"use strict";
exports=module.exports;

var Errors = require('./errors');
var VariableTerm = require('./variable_term.js');
var AtomTerm = require('./atom_term.js');
var IntegerTerm = require('./integer_term.js');
var CompoundTerm = require('./compound_term.js');
var Constants = require('./constants.js');
var Errors = require('./errors.js');
var CTable = require('./ctable.js');

// FIXME: This is not very efficient - it would be much better as a linked list
var nextRef = 0;
var database = [];

function get_index(key)
{
    if (TAGOF(key) == VariableTag)
        Errors.instantiationError(key);
    else if (TAGOF(key) == ConstantTag)
        return "K" + key;
    else if (TAGOF(key) == CompoundTag)
    {
        var functor = CTable.get(FUNCTOROF(key));
        return "C" + functor.name + "/" + functor.arity; // Note that functor.name is a Ctable ref - a number. But it is constant, at least
    }
    else
        Errors.typeError(Constants.keyAtom, key);
}

module.exports.recorda = function(key, term, ref)
{
    var index = get_index(key);
    if (!this.unify(ref, IntegerTerm.get(nextRef)))
        return false;
    database.unshift({index: index,
                   key: this.copyTerm(key),
                   ref: nextRef,
                   value: this.copyTerm(term)});
    nextRef++;
    return true;
}

module.exports.recordz = function(key, term, ref)
{
    var index = get_index(key);
    if (!this.unify(ref, IntegerTerm.get(nextRef)))
        return false;
    database.push({index: index,
                    key: this.copyTerm(key),
                    ref: nextRef,
                    value: this.copyTerm(term)});
    nextRef++;
    return true;
}

module.exports.recorded = function(key, value, ref)
{
    if (TAGOF(ref) == ConstantTag && CTable.get(ref) instanceof IntegerTerm)
    {
        // Deterministic case
        needle = CTable.get(ref).value;
        for (var i = 0; i < database.length; i++)
        {
            if (database[i] === undefined) // tombstone
                continue;
            if (database[i].ref == needle)
            {
                return this.unify(key, database[i].key) && this.unify(value, database[i].value);
            }
        }
    }
    else if (TAGOF(ref) == VariableTag)
    {
        var index = this.foreign || 0;
        while (database[index] === undefined && index < database.length)
            index++;
        if (index >= database.length)
            return false;
        if (database.length > index + 1)
        {
            //console.log("Leaving a choicepoint");
            this.create_choicepoint(index+1);
        }
        else
        {
            //console.log("No other choices (" + index + ", " + database.length + ")");
        }
        //console.log("Returning a value: " + database[index].value);
        return this.unify(key, database[index].key) && this.unify(value, database[index].value) && this.unify(ref, IntegerTerm.get(database[index].ref));
    }
    else
        Errors.typeError(Constants.dbReferenceAtom, key);
}

module.exports.erase = function(ref)
{
    if (TAGOF(ref) == VariableTag)
        Errors.instantiationError(key);
    else if (TAGOF(ref) == ConstantTag && CTable.get(ref) instanceof IntegerTerm)
        ref = CTable.get(ref).value;
    else
        Errors.typeError(Constants.dbReferenceAtom, key);
    for (var i = 0; i < database.length; i++)
    {
        if (database[i] == undefined)
            continue;
        if (database[i].ref == ref)
        {
            database[i] = undefined;
            return true;
        }
    }
    return false;
}
