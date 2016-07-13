var module_map = [];
var Compiler = require('./compiler.js');
var Functor = require('./functor.js');
var Errors = require('./errors.js');
var AtomTerm = require('./atom_term.js');
var IntegerTerm = require('./integer_term.js');
var Term = require('./term.js');
var Constants = require('./constants.js');
var CompoundTerm = require('./compound_term.js');
var util = require('util');

function Module(name)
{
    this.name = name;
    this.predicates = {};
}

Module.prototype.defineForeignPredicate = function(name, fn)
{
    var functor = new Functor(new AtomTerm(name), fn.length);
    var compiled = Compiler.foreignShim(fn);
    this.predicates[functor.toString()] = {firstClause: compiled,
                                           clauses: [],
                                           foreign: true,
                                           functor: functor};
    console.log(">>> Defined (foreign) " + this.name + ":" + functor);
}

Module.prototype.definePredicate = function(functor)
{
    this.predicates[functor.toString()] = {clauses: [],
                                           firstClause: undefined,
                                           functor: functor};
    console.log(">>> Defined " + this.name + ":" + functor);
}

Module.prototype.makeDynamic = function(functor)
{
    // FIXME: Do not allow adding clauses to compiled predicates?
    if (this.predicates[functor.toString()] === undefined)
        this.definePredicate(functor);
    else if (this.predicates[functor.toString()].dynamic !== true)
        Errors.permissionError(Constants.modifyAtom, Constants.staticProcedureAtom, new CompoundTerm(Constants.predicateIndicatorFunctor, [functor.name, new IntegerTerm(functor.arity)]));
    this.predicates[functor.toString()].dynamic = true;
}

Module.prototype.asserta = function(term)
{
    var functor = Term.clause_functor(term);
    this.makeDynamic(functor);
    // FIXME: also, compile it and add it to the start of the clauses.
    this.predicates[functor.toString()].clauses.unshift(term);
    this.getPredicateCode(functor);
}

Module.prototype.assertz = function(term)
{
    var functor = Term.clause_functor(term);
    this.makeDynamic(functor);
    // FIXME: also, compile it and add it to the end of the clauses.
    this.predicates[functor.toString()].clauses.push(term);
    this.getPredicateCode(functor);
}

Module.prototype.retractClause = function(functor, index)
{
    // FIXME: also, cut it out of the middle
    this.predicates[functor.toString()].clauses.splice(index, 1);
    this.getPredicateCode(functor);
}

Module.prototype.abolish = function(indicator)
{
    Term.must_be_pi(indicator);
    var functor = new Functor(indicator.args[0], indicator.args[1].value);
    if (this.predicates[functor.toString()] === undefined || this.predicates[functor.toString()].dynamic !== true)
        Errors.permissionError(Constants.modifyAtom, Constants.staticProcedureAtom, indicator);
    this.predicates[functor.toString()] = undefined;
}

Module.prototype.addClause = function(functor, clause)
{
    // FIXME: Check that this is not an access violation (for example, writing to ','/2)
    if (functor.equals(Constants.conjunctionFunctor))
        throw new Error("Cannot change ,/2: " + clause);
    if (this.predicates[functor.toString()] === undefined)
        this.definePredicate(functor);
    this.predicates[functor.toString()].clauses.push(clause);
}

Module.prototype.addClausea = function(functor, clause)
{
    if (this.predicates[functor.toString()] === undefined)
        this.definePredicate(functor);
    this.predicates[functor.toString()].clauses.push(clause);
}

Module.prototype.compilePredicate = function(functor)
{
    this.predicates[functor.toString()].firstClause = Compiler.compilePredicate(this.predicates[functor.toString()].clauses);
}

Module.prototype.getPredicateCode = function(functor)
{
    //console.log("Looking for " + this.name + ":" + functor);
    if (this.predicates[functor.toString()] === undefined)
        return undefined;
    if (this.predicates[functor.toString()].firstClause === undefined)
        this.compilePredicate(functor);
    //console.log(util.inspect(this.predicates[functor.toString()], {showHidden: false, depth: null}));
    return this.predicates[functor.toString()].firstClause;
}

Module.get = function(name)
{
    if (module_map[name] === undefined)
        module_map[name] = new Module(name);
    return module_map[name];
}

module.exports = Module;
