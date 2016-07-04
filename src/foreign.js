var ArrayUtils = require('./array_utils.js');
var Constants = require('./constants.js');
var CompoundTerm = require('./compound_term.js');
var AtomTerm = require('./atom_term.js');
var VariableTerm = require('./variable_term.js');

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

module.exports.writeln = function(arg)
{
    var bytes = ArrayUtils.toByteArray(arg.toString());
    console.log(bytes);
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

module.exports["=.."] = function(term, univ)
{
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
    // FIXME: Else, type error?

}
