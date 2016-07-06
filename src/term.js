var Constants = require('./constants.js');
var CompoundTerm = require('./compound_term.js');
var AtomTerm = require('./atom_term.js');
var BlobTerm = require('./blob_term.js');
var VariableTerm = require('./variable_term.js');
var IntegerTerm = require('./integer_term.js');
var FloatTerm = require('./float_term.js');
var Errors = require('./errors.js');

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

module.exports.must_be_pi = function(type, t)
{
    module.exports.must_be_bound(t);
    if (!(t instanceof CompoundTerm))
        Errors.typeError(Constants.predicateIndicatorAtom, t);
    if (!t.functor.equals(Constants.predicateIndicatorFunctor))
        Errors.typeError(Constants.predicateIndicatorAtom, t);
}

// Given a term, check that is is callabale, and return the functor of the term
// eg foo(a,b) -> foo/2
//    foo      -> foo/0
//    3        -> type_error(callable, 3)
module.exports.head_functor = function(t)
{
    if (t instanceof AtomTerm)
        return new Functor(t, 0);
    else if (t instanceof CompoundTerm)
        return t.functor;
    Errors.typeError(Constants.callableAtom, t);
}


// Given a term representing a clause, return the functor of the clause
// eg foo(a,b) :- bar, baz, qux     -> foo/2
//    foo(a,b)                      -> foo/2
//    foo:- bar ; baz               -> foo/0
module.exports.clause_functor = function(t)
{
    if (t instanceof AtomTerm)
        return new Functor(t, 0);
    else if (t instanceof CompoundTerm)
    {
        if (t.functor.equals(Constants.clauseFunctor))
            return module.exports.head_functor(t.args[0]);
        return t.functor;
    }
    Errors.typeError(Constants.callableAtom, t);
}
