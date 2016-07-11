var Errors = require('./errors');
var Kernel = require('./kernel');
var VariableTerm = require('./variable_term.js');
var AtomTerm = require('./atom_term.js');
var IntegerTerm = require('./integer_term.js');
var CompoundTerm = require('./compound_term.js');
var util = require('util');

// FIXME: This is not very efficient - it would be much better as a linked list
var nextRef = 0;
var database = [];

function get_index(key)
{
    if (key instanceof VariableTerm)
        Errors.instantiationError(key);
    if (key instanceof AtomTerm)
        return "A" + key.value;
    else if (key instanceof IntegerTerm)
        return "I" + key.value;
    else if (key instanceof CompoundTerm)
        return "C" + key.functor.name.value + "/" + key.functor.arity;
    else
        Errors.typeError(Constants.keyAtom, key);
}

module.exports.recorda = function(key, term, ref)
{
    var index = get_index(key);
    if (!this.unify(ref, new IntegerTerm(nextRef)))
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
    if (!this.unify(ref, new IntegerTerm(nextRef)))
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
    if (ref instanceof IntegerTerm)
    {
        // Deterministic case
        for (var i = 0; i < database.length; i++)
        {
            if (database[i] === undefined) // tombstone
                continue;
            if (database[i].ref == ref)
            {
                return this.unify(key, database[i].key) && this.unify(value, database[i].value);
            }
        }
    }
    else if (ref instanceof VariableTerm)
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
        return this.unify(key, database[index].key) && this.unify(value, database[index].value) && this.unify(ref, new IntegerTerm(database[index].ref));
    }
    else
        Errors.typeError(Constants.dbReferenceAtom, key);
}

module.exports.erase = function(ref)
{
    if (ref instanceof VariableTerm)
        Errors.instantiationError(key);
    else if (ref instanceof IntegerTerm)
        ref = ref.value;
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
