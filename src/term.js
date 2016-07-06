var Constants = require('./constants.js');
var CompoundTerm = require('./compound_term.js');
var AtomTerm = require('./atom_term.js');
var BlobTerm = require('./blob_term.js');
var VariableTerm = require('./variable_term.js');
var IntegerTerm = require('./integer_term.js');
var FloatTerm = require('./float_term.js');

module.exports.must_be_bound = function(t)
{
    if (t instanceof VariableTerm)
        Errors.instantiationError(t);
}

module.exports.must_be_atom = function(t)
{
    module.exports.must_be_bound(t);
    if (!(t instanceof AtomTerm))
        Errors.typeError(Constants.atomAtom, t);
}

module.exports.must_be_integer = function(t)
{
    if (!(t instanceof IntegerTerm))
        Errors.typeError(Constants.atomAtom, t);
}

module.exports.must_be_character = function(t)
{
    module.exports.must_be_bound(t);
    if (!(t instanceof AtomTerm))
        Errors.typeError(Constants.characterAtom, t);
    if (t.value.length != 1)
        Errors.typeError(Constants.characterAtom, t);
}

module.exports.must_be_blob = function(type, t)
{
    module.exports.must_be_bound(t);
    if (!(t instanceof BlobTerm))
        Errors.typeError(new AtomTerm(type), t);
    if (t.type != type)
        Errors.typeError(new AtomTerm(type), t);
}

module.exports.head_functor = function(t)
{
    if (t instanceof AtomTerm)
        return new Functor(t, 0);
    else if (t instanceof CompoundTerm)
        return t.functor;
    Errors.typeError(Constants.callableAtom, t);
}
