"use strict";

var Constants = require('./constants.js');
var CompoundTerm = require('./compound_term.js');
var AtomTerm = require('./atom_term.js');
var BlobTerm = require('./blob_term.js');
var VariableTerm = require('./variable_term.js');
var IntegerTerm = require('./integer_term.js');
var Functor = require('./functor.js');
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

module.exports.must_be_compound = function(t)
{
    module.exports.must_be_bound(t);
    if (!(t instanceof CompoundTerm))
        Errors.typeError(Constants.compoundAtom, t);
}

module.exports.must_be_atomic = function(t)
{
    module.exports.must_be_bound(t);
    if (!((t instanceof AtomTerm) ||
          (t instanceof IntegerTerm) ||
          (t instanceof FloatTerm)))
        Errors.typeError(Constants.atomicAtom, t);
}

module.exports.must_be_integer = function(t)
{
    module.exports.must_be_bound(t);
    if (!(t instanceof IntegerTerm))
        Errors.typeError(Constants.integerAtom, t);
}

module.exports.must_be_character_code = function(t)
{
    if (!(t instanceof IntegerTerm) || (t.value < 0 || t.value > 65535))
        Errors.representationError(Constants.characterCodeAtom);
}

module.exports.must_be_positive_integer = function(t)
{
    module.exports.must_be_integer(t);
    if (t.value < 0)
        Errors.domainError(Constants.notLessThanZeroAtom, t);
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

module.exports.must_be_pi = function(t)
{
    module.exports.must_be_bound(t);
    if (!(t instanceof CompoundTerm))
        Errors.typeError(Constants.predicateIndicatorAtom, t);
    if (!t.functor.equals(Constants.predicateIndicatorFunctor))
        Errors.typeError(Constants.predicateIndicatorAtom, t);
    module.exports.must_be_atom(t.args[0]);
    module.exports.must_be_positive_integer(t.args[1]);
}

// Given a term, check that is is callable, and return the functor of the term
// eg foo(a,b) -> foo/2
//    foo      -> foo/0
//    3        -> type_error(callable, 3)
module.exports.head_functor = function(t)
{
    module.exports.must_be_bound(t);
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
    module.exports.must_be_bound(t);
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


// Convenience function that returns a Prolog term corresponding to the given list of prolog terms.
module.exports.from_list = function(list, tail)
{
    var result = tail || Constants.emptyListAtom;
    for (var i = list.length-1; i >= 0; i--)
        result = new CompoundTerm(Constants.listFunctor, [list[i], result]);
    return result;
}

// Function that returns a Javascript list of the terms in the prolog list. Raises an error if the list is not a proper list
module.exports.to_list = function(term)
{
    var list = [];
    while (term instanceof CompoundTerm && term.functor.equals(Constants.listFunctor))
    {
        list.push(term.args[0]);
        term = term.args[1].dereference();
    }
    if (!term.equals(Constants.emptyListAtom))
        Errors.typeError(Constants.listAtom, term);
    return list;
}

module.exports.predicate_indicator = function(term)
{
    if (term instanceof AtomTerm)
        return new CompoundTerm(Constants.predicateIndicatorFunctor, [term, new IntegerTerm(0)]);
    return new CompoundTerm(Constants.predicateIndicatorFunctor, [term.functor.name, new IntegerTerm(term.functor.arity)]);
}

module.exports.difference = function(a, b)
// Computes a-b, so that if a is bigger than b, the result is positive
{
    a = a.dereference();
    b = b.dereference();
    if (a.equals(b))
    {
        return 0;
    }
    if (a instanceof VariableTerm)
    {
        if (b instanceof VariableTerm)
            return a.index - b.index;
        return -1;
    }
    if (a instanceof FloatTerm)
    {
        if (b instanceof VariableTerm)
            return 1;
        else if (b instanceof FloatTerm)
            return a.value - b.value;
        return -1;
    }
    if (a instanceof IntegerTerm)
    {
        if ((b instanceof VariableTerm) || (b instanceof FloatTerm))
            return 1;
        else if (b instanceof IntegerTerm)
            return a.value - b.value;
        return -1;
    }
    if (a instanceof AtomTerm)
    {
        if ((b instanceof VariableTerm) || (b instanceof FloatTerm) || (b instanceof IntegerTerm))
            return 1;
        else if (b instanceof AtomTerm)
            return (a.value > b.value)?1:-1;
        return -1;
    }
    if (a instanceof CompoundTerm)
    {
        if ((b instanceof VariableTerm) || (b instanceof FloatTerm) || (b instanceof IntegerTerm) || (b instanceof AtomTerm))
            return 1;
        if (b instanceof CompoundTerm)
        {
            if (a.functor.arity != b.functor.arity)
                return a.functor.arity - b.functor.arity;
            if (a.functor.name.value != b.functor.name.value)
                return a.functor.name.value>b.functor.name.value?1:-1;
            for (var i = 0; i < a.functor.arity; i++)
            {
                var d = module.exports.difference(a.args[i], b.args[i]);
                if (d != 0)
                    return d;
            }
            return 0;
        }
        return -1;
    }
    // FIXME: Other types here?
}
