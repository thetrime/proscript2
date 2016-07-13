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
    this.predicates[functor.toString()] = {firstClause: Compiler.foreignShim(fn),
                                           clauses: [],
                                           foreign: true,
                                           dynamic: false,
                                           functor: functor};
    console.log(">>> Defined (foreign) " + this.name + ":" + functor);
}

Module.prototype.definePredicate = function(functor)
{
    this.predicates[functor.toString()] = {clauses: [],
                                           firstClause: null,
                                           foreign: false,
                                           dynamic: false,
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
    // Dynamic clauses ALWAYS need the choicepoint since they can change as execution progresses
    var clause = Compiler.compilePredicateClause(term, true);
    clause.nextClause = this.predicates[functor.toString()].firstClause;
    this.predicates[functor.toString()].firstClause = clause;
    // Also store it in the clauses array so we can find it again later for unification
    this.predicates[functor.toString()].clauses.unshift(term);
}

Module.prototype.assertz = function(term)
{
    var functor = Term.clause_functor(term);
    this.makeDynamic(functor);

    // If there are no clauses then assertz is the same as asserta, and the code in asserta is simpler
    if (this.predicates[functor.toString()].firstClause == null)
        this.asserta(term);

    var clause = Compiler.compilePredicateClause(term, true);
    this.predicates[functor.toString()].clauses.push(term);
    // We only get here if there are at least some clauses
    for (var c = this.predicates[functor.toString()].firstClause; c != null; c = c.nextClause)
    {
        if (c.nextClause == null)
        {
            c.nextClause = clause;
            break;
        }
    }
}

Module.prototype.retractClause = function(functor, index)
{
    this.predicates[functor.toString()].clauses.splice(index, 1);
    // This is the trickiest operation
    if (index == 0)
        this.predicates[functor.toString()].firstClause = this.predicates[functor.toString()].firstClause.nextClause;
    else
    {
        var c = this.predicates[functor.toString()].firstClause;
        for (var i = 0; i < index-1; i++)
            c = c.nextClause;
        c.nextClause = c.nextClause.nextClause;
    }
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
    // FIXME: This should only really happen for static predicates
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
    if (this.predicates[functor.toString()].firstClause === null)
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
