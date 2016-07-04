var ArrayUtils = require('./array_utils.js');
var Constants = require('./constants.js');
var CompoundTerm = require('./compound_term.js');
var AtomTerm = require('./atom_term.js');
var VariableTerm = require('./variable_term.js');
var IntegerTerm = require('./integer_term.js');
var FloatTerm = require('./float_term.js');

// Convenience function that returns a Prolog term corresponding to the given list of prolog terms.
function term_from_list(list, tail)
{
    var result = tail || Constants.emptyListAtom;
    for (var i = list.length-1; i >= 0; i--)
        result = new CompoundTerm(Constants.listFunctor, [list[i], result]);
    console.log("term_from_list-> " + result);
    return result;
}

// Function that returns a Javascript list of the terms in the prolog list. Raises an error if the list is not a proper list
function list_from_term(term)
{
    var list = [];
    while (term instanceof CompoundTerm && term.functor.equals(Constants.listFunctor))
    {
        list.push(term.args[0]);
        term = term.args[1];
    }
    if (!term.equals(Constants.emptyListAtom))
        throw new Error("Not a proper list");
}

// ISO built-in predicates
// 8.2.1
module.exports["="] = function(a, b)
{
    return this.unify(a,b);
}
// 8.2.2
module.exports.unify_with_occurs_check = function(a, b)
{
    throw new Error("FIXME: Not implemented");
}
// 8.2.4
module.exports["\\="] = function(a, b)
{
    throw new Error("FIXME: Not implemented");
}
// 8.3.1
module.exports["var"] = function(a)
{
    return (a instanceof VariableTerm);
}
// 8.3.2
module.exports["atom"] = function(a)
{
    return (a instanceof AtomTerm);
}
// 8.3.3
module.exports["integer"] = function(a)
{
    return (a instanceof IntegerTerm);
}
// 8.3.4
module.exports["float"] = function(a)
{
    return (a instanceof FloatTerm);
}
// 8.3.5
module.exports.atomic = function(a)
{
    return ((a instanceof AtomTerm) ||
            (a instanceof IntegerTerm) ||
            (a instanceof FloatTerm));
    // FIXME: Bignum support
}
// 8.3.6
module.exports.compound = function(a)
{
    return (a instanceof CompoundTerm);
}
// 8.3.7
module.exports.nonvar = function(a)
{
    return !(a instanceof VariableTerm);
}
// 8.3.8
module.exports.number = function(a)
{
    return ((a instanceof IntegerTerm) ||
            (a instanceof FloatTerm));
    // FIXME: Bignum support
}
// 8.4.1
module.exports["@=<"] = function(a, b)
{
    throw new Error("FIXME: Not implemented");
}
module.exports["=="] = function(a, b)
{
    throw new Error("FIXME: Not implemented");
}
module.exports["\=="] = function(a, b)
{
    throw new Error("FIXME: Not implemented");
}
module.exports["@<"] = function(a, b)
{
    throw new Error("FIXME: Not implemented");
}
module.exports["@>"] = function(a, b)
{
    throw new Error("FIXME: Not implemented");
}
module.exports["@>="] = function(a, b)
{
    throw new Error("FIXME: Not implemented");
}
// 8.5.1
module.exports.functor = function(term, name, arity)
{
    if (term instanceof VariableTerm)
    {
        // Construct a term
        if (arity instanceof VariableTerm)
            Errors.instantiationError(arity);
        if (arity instanceof VariableTerm)
            Errors.instantiationError(arity);
        if (!(arity instanceof IntegerTerm))
            Errors.typeError(Constants.integerAtom, arity);
        var args = new Array(arity.value);
        for (var i = 0; i < args.length; i++)
            args[i] = new VariableTerm();
        console.log(name + ", " + args);
        return this.unify(term, new CompoundTerm(name, args));
    }
    if (term instanceof AtomTerm)
    {
        return this.unify(name, term) && this.unify(arity, new IntegerTerm(0));
    }
    if (term instanceof IntegerTerm)
    {
        return this.unify(name, term) && this.unify(arity, new IntegerTerm(0));
    }
    if (term instanceof CompoundTerm)
    {
        return this.unify(name, term.functor.name) && this.unify(arity, new IntegerTerm(term.functor.arity));
    }
    else
        Errors.typeError(Constants.termAtom, term);
}
// 8.5.2
module.exports.arg = function(n, term, arg)
{
    // NB: ISO only requires the +,+,? mode
    if (n instanceof VariableTerm)
        Errors.instantiationError(n);
    if (term instanceof VariableTerm)
        Errors.instantiationError(term);
    if (!(n instanceof IntegerTerm))
        Errors.typeError(Constants.integerAtom, n);
    if (n.value < 0)
        Errors.domainError(Constants.notLessThanZeroAtom, n);
    if (!(term instanceof CompoundTerm))
        Errors.typeError(Constants.compound, term);
    if (term.args[n.value] === undefined)
        return false; // N is too big
    return this.unify(term.args[n.value], arg);
}
// 8.5.3
module.exports["=.."] = function(term, univ)
{
    // CHECKME: More errors should be checked
    if (term instanceof AtomTerm) // FIXME: Also other atomic-type terms
        return this.unify(univ, term_from_list([term]));
    if (term instanceof CompoundTerm)
    {
        return this.unify(univ, term_from_list([term.functor.name].concat(term.args)));
    }
    if (term instanceof VariableTerm)
    {
        var list = list_from_term(univ);
        var fname = list.unshift();
        return this.unify(term, new CompoundTerm(fname, list));
    }
    else
        Errors.typeError(Constants.termAtom, term);
}
// 8.5.4
module.exports.copy_term = function(term, copy)
{
    throw new Error("FIXME: Not implemented");
}
// 8.8.1
module.exports.clause = function(head, body)
{
    throw new Error("FIXME: Not implemented");
}
// 8.8.2
module.exports.current_predicate = function(head, body)
{
    throw new Error("FIXME: Not implemented");
}
// 8.9 in iso_record.js

// 8.10.1
module.exports.findall = function(template, goal, instances)
{
    throw new Error("FIXME: Not implemented");
}
// 8.10.2
module.exports.bagof = function(template, goal, instances)
{
    throw new Error("FIXME: Not implemented");
}
// 8.10.3
module.exports.setof = function(template, goal, instances)
{
    throw new Error("FIXME: Not implemented");
}
// 8.11 in iso_stream.js
// 8.12 in iso_stream.js
// 8.13 in iso_stream.js
// 8.14 in iso_stream.js

// 8.15.1: \+ is compiled directly to opcodes
// 8.15.2
module.exports.once = function(goal)
{
    throw new Error("FIXME: Not implemented");
}
// 8.15.3
module.exports.repeat = function()
{
    throw new Error("FIXME: Not implemented");
}
// 8.16.1
module.exports.atom_length = function(atom, length)
{
    if (atom instanceof VariableTerm)
        Errors.instantiationError(atom)
    if (!(atom instanceof AtomTerm))
        Errors.typeError(Constants.atomAtom, atom)
    if (!((length instanceof IntegerTerm) || length instanceof VariableTerm))
        Errors.typeError(Constants.integerAtom, length)
    if ((length instanceof IntegerTerm) && length.value < 0)
        Errors.domainError(Constants.notLessThanZeroAtom, length)
    return this.unify(new IntegerTerm(atom.value.length), length);
}
// 8.16.2
module.exports.atom_concat = function(a, b, c)
{
    // ??+ or ++-
    throw new Error("FIXME: Not implemented");
}
// 8.16.3
module.exports.sub_atom = function(atom, before, length, after, subatom)
{
    // This is really hard to get right
    throw new Error("FIXME: Not implemented");
}
// 8.16.4
module.exports.atom_chars = function(atom, chars)
{
    throw new Error("FIXME: Not implemented");
}
// 8.16.5
module.exports.atom_codes = function(atom, codes)
{
    throw new Error("FIXME: Not implemented");
}
// 8.16.6
module.exports.char_code = function(c, code)
{
    throw new Error("FIXME: Not implemented");
}
// 8.16.7
module.exports.number_chars = function(number, chars)
{
    throw new Error("FIXME: Not implemented");
}
// 8.16.8
module.exports.number_codes = function(number, codes)
{
    throw new Error("FIXME: Not implemented");
}
// 8.17.1
module.exports.set_prolog_flag = function(flag, value)
{
    throw new Error("FIXME: Not implemented");
}
// 8.17.2
module.exports.current_prolog_flag = function(flag, value)
{
    throw new Error("FIXME: Not implemented");
}
// 8.17.3
module.exports.halt = [function()
                       {
                           throw new Error("Not implemented");
                       },
// 8.17.4
                       function(a)
                       {
                           throw new Error("Not implemented");
                       }];

