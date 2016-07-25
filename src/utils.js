"use strict";
exports = module.exports;

var Constants = require('./constants.js');
var CompoundTerm = require('./compound_term.js');
var AtomTerm = require('./atom_term.js');
var BlobTerm = require('./blob_term.js');
var VariableTerm = require('./variable_term.js');
var IntegerTerm = require('./integer_term.js');
var Functor = require('./functor.js');
var FloatTerm = require('./float_term.js');
var Errors = require('./errors.js');
var CTable = require('./ctable.js');

module.exports.must_be_bound = function(t)
{
    if (TAGOF(DEREF(t)) == VariableTag)
        Errors.instantiationError(t);
}

module.exports.must_be_atom = function(t)
{
    module.exports.must_be_bound(t);
    if (!(TAGOF(DEREF(t)) == ConstantTag && CTable.get(t) instanceof AtomTerm))
        Errors.typeError(Constants.atomAtom, t);
}

module.exports.must_be_compound = function(t)
{
    module.exports.must_be_bound(t);
    if (!(TAGOF(DEREF(t)) == CompoundTag))
        Errors.typeError(Constants.compoundAtom, t);
}

module.exports.must_be_atomic = function(t)
{
    module.exports.must_be_bound(t);
    if (TAGOF(t) != ConstantTag)
        Errors.typeError(Constants.atomicAtom, t);
}

module.exports.must_be_integer = function(t)
{
    module.exports.must_be_bound(t);
    if (!(TAGOF(DEREF(t)) == ConstantTag && CTable.get(DEREF(t)) instanceof IntegerTerm))
        Errors.typeError(Constants.integerAtom, t);
}


module.exports.must_be_character_code = function(t)
{
    module.exports.must_be_integer(t);
    var t1 = CTable.get(DEREF(t));
    if ((t1.value < 0 || t1.value > 65535))
        Errors.representationError(Constants.characterCodeAtom);
}

module.exports.must_be_positive_integer = function(t)
{
    module.exports.must_be_integer(t);
    if (CTable.get(DEREF(t)).value < 0)
        Errors.domainError(Constants.notLessThanZeroAtom, t);
}

module.exports.must_be_character = function(t)
{
    if (!(TAGOF(DEREF(t)) == ConstantTag && CTable.get(DEREF(t)) instanceof AtomTerm))
        Errors.typeError(Constants.characterAtom, t);
    if (CTable.get(DEREF(t)).value.length != 1)
        Errors.typeError(Constants.characterAtom, t);
}

module.exports.must_be_blob = function(type, t)
{
    module.exports.must_be_bound(t);
    if (!(TAGOF(DEREF(t)) == ConstantTag && CTable.get(DEREF(t)) instanceof BlobTerm))
        Errors.typeError(AtomTerm.get(type), t);
    if (CTable.get(DEREF(t)).type != type)
        Errors.typeError(AtomTerm.get(type), t);
}

module.exports.must_be_pi = function(t)
{
    module.exports.must_be_bound(t);
    if (TAGOF(DEREF(t)) != CompoundTag)
        Errors.typeError(Constants.predicateIndicatorAtom, t);
    if (FUNCTOROF(DEREF(t) != Constants.predicateIndicatorFunctor))
        Errors.typeError(Constants.predicateIndicatorAtom, t);
    module.exports.must_be_atom(ARGOF(t, 0));
    module.exports.must_be_positive_integer(ARGOF(t, 1));
}

// Given a term, check that is is callable, and return the functor of the term
// eg foo(a,b) -> foo/2
//    foo      -> foo/0
//    3        -> type_error(callable, 3)
module.exports.head_functor = function(t)
{
    module.exports.must_be_bound(t);
    if (TAGOF(DEREF(t)) == CompoundTag)
        return FUNCTOROF(DEREF(t));
    else if (TAGOF(DEREF(t)) == ConstantTag && CTable.get(DEREF(t)) instanceof AtomTerm)
        return Functor.get(t, 0);
    Errors.typeError(Constants.callableAtom, t);
}


// Given a term representing a clause, return the functor of the clause
// eg foo(a,b) :- bar, baz, qux     -> foo/2
//    foo(a,b)                      -> foo/2
//    foo:- bar ; baz               -> foo/0
module.exports.clause_functor = function(t)
{
    module.exports.must_be_bound(t);
    if (TAGOF(t) == ConstantTag && CTable.get(t) instanceof AtomTerm)
        return Functor.get(t, 0);
    else if (TAGOF(t) == CompoundTag)
    {
        if (FUNCTOROF(t) == Constants.clauseFunctor)
            return module.exports.head_functor(ARGOF(t, 0));
        return FUNCTOROF(t);
    }
    Errors.typeError(Constants.callableAtom, t);
}


// Convenience function that returns a Prolog term corresponding to the given list of prolog terms.
module.exports.from_list = function(list, tail)
{
    var result = tail || Constants.emptyListAtom;
    for (var i = list.length-1; i >= 0; i--)
        result = CompoundTerm.create(Constants.listFunctor, [list[i], result]);
    return result;
}

// Function that returns a Javascript list of the terms in the prolog list. Raises an error if the list is not a proper list
module.exports.to_list = function(term)
{
    var list = [];
    while (TAGOF(term) == CompoundTag && FUNCTOROF(term) == Constants.listFunctor)
    {
        list.push(ARGOF(term, 0));
        term = ARGOF(term, 1);
    }
    if (term != Constants.emptyListAtom)
        Errors.typeError(Constants.listAtom, term);
    return list;
}

module.exports.predicate_indicator = function(term)
{
    if (TAGOF(term) == ConstantTag && CTable.get(term) instanceof AtomTerm)
        return CompoundTerm.create(Constants.predicateIndicatorFunctor, [term, IntegerTerm.get(0)]);
    if (TAGOF(term) == CompoundTag)
        return CompoundTerm.create(Constants.predicateIndicatorFunctor, [CTable.get(FUNCTOROF(term)).name, IntegerTerm.get(CTable.get(FUNCTOROF(term)).arity)]);
}

module.exports.difference = function(a, b)
// Computes a-b, so that if a is bigger than b, the result is positive
{
    a = DEREF(a);
    b = DEREF(b);
    if (a == b)
    {
        return 0;
    }
    if (TAGOF(a) == VariableTag)
    {
        if (TAGOF(b) == VariableTag)
        {
            return a - b;
        }
        return -1;
    }
    if (TAGOF(a) == ConstantTag)
    {
        a = CTable.get(a);
        if (a instanceof FloatTerm)
        {
            if (TAGOF(b) == VariableTag)
                return 1;
            else if (TAGOF(b) == ConstantTag)
            {
                if (CTable.get(b) instanceof FloatTerm)
                    return a.value - CTable.get(b).value;
            }
            return -1;
        }
        if (a instanceof IntegerTerm)
        {
            if (TAGOF(b) == VariableTag)
                return 1;
            else if (TAGOF(b) == ConstantTag)
            {
                b = CTable.get(b);
                if (CTable.get(b) instanceof FloatTerm)
                    return 1;
                else if (b instanceof IntegerTerm)
                    return a.value - b.value;
            }
            return -1;
        }
        if (a instanceof AtomTerm)
        {
            if (TAGOF(b) == VariableTag)
                return 1;
            else if (TAGOF(b) == ConstantTag)
            {
                b = CTable.get(b);
                if ((b instanceof FloatTerm) || (b instanceof IntegerTerm))
                    return 1;
                else if (b instanceof AtomTerm)
                    return (a.value > b.value)?1:-1;
            }
            return -1;
        }
    }
    if (TAGOF(a) == CompoundTag)
    {
        if (TAGOF(b) != CompoundTag)
            return 1;
        if (TAGOF(b) == CompoundTag)
        {
            fa = CTable.get(FUNCTOROF(a));
            fb = CTable.get(FUNCTOROF(b));
            if (fa.arity != fb.arity)
                return fa.arity - fb.arity;
            if (fa.name != fb.name)
                return CTable.get(fa.name).value > CTable.get(fb.name).value?1:-1;
            for (var i = 0; i < fa.arity; i++)
            {
                var d = module.exports.difference(ARGOF(a, i), ARGOF(b, i));
                if (d != 0)
                    return d;
            }
            return 0;
        }
        return -1;
    }
}
