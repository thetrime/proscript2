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


module.exports.writeln = function(arg)
{
    var bytes = ArrayUtils.toByteArray(arg.toString());
    return this.streams.stdout.write(this.streams.stdout, 1, bytes.length, bytes) >= 0;
}

module.exports.fail = function()
{
    return false;
}

module.exports.true = function()
{
    return true;
}

module.exports.term_variables = function(t, vt)
{
    return this.unify(term_from_list(term_variables(t), Constants.emptyListAtom), vt);
}

module.exports.halt = function()
{
    throw new Error("Not implemented");
}



