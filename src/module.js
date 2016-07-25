"use strict";
exports=module.exports;

var Compiler = require('./compiler.js');
var Functor = require('./functor.js');
var Errors = require('./errors.js');
var AtomTerm = require('./atom_term.js');
var IntegerTerm = require('./integer_term.js');
var Utils = require('./utils.js');
var Constants = require('./constants.js');
var CompoundTerm = require('./compound_term.js');
var CTable = require('./ctable.js');

function Module(name)
{
    this.name = name;
    this.term = AtomTerm.get(name);
    this.predicates = {};
}

Module.prototype.defineForeignPredicate = function(name, fn)
{
    var functor = Functor.get(AtomTerm.get(name), fn.length);
    this.predicates[functor] = {firstClause: Compiler.foreignShim(fn),
                                           clauses: [],
                                           meta: false,
                                           foreign: true,
                                           dynamic: false,
                                           functor: functor};
    //console.log(">>> Defined (foreign) " + this.name + ":" + functor);
}

Module.prototype.definePredicate = function(functor)
{
    this.predicates[functor] = {clauses: [],
                                           firstClause: null,
                                           foreign: false,
                                           dynamic: false,
                                           meta: false,
                                           functor: functor};
    //console.log(">>> Defined " + this.name + ":" + functor);
}

Module.prototype.makeDynamic = function(functor)
{
    // FIXME: Do not allow adding clauses to compiled predicates?
    if (this.predicates[functor] === undefined)
        this.definePredicate(functor);
    else if (this.predicates[functor].dynamic !== true)
        Errors.permissionError(Constants.modifyAtom, Constants.staticProcedureAtom, CompoundTerm.create(Constants.predicateIndicatorFunctor, [functor.name, IntegerTerm.get(functor.arity)]));
    this.predicates[functor].dynamic = true;
}

Module.prototype.makeMeta = function(functor, args)
{
    if (this.predicates[functor] === undefined)
        this.definePredicate(functor);
    this.predicates[functor].meta = args;
    // Recompile if there are already clauses
    if (this.predicates[functor].clauses.length > 0)
        this.compilePredicate(functor);
}

Module.prototype.asserta = function(term)
{
    var functor = Utils.clause_functor(term);
    this.makeDynamic(functor);
    // Dynamic clauses ALWAYS need the choicepoint since they can change as execution progresses
    var clause = Compiler.compilePredicateClause(term, true, this.predicates[functor].meta);
    clause.nextClause = this.predicates[functor].firstClause;
    this.predicates[functor].firstClause = clause;
    // Also store it in the clauses array so we can find it again later for unification
    this.predicates[functor].clauses.unshift(term);
    //console.log("After asserting, the predicate has " + this.predicates[functor].clauses.length + " clauses");
}

Module.prototype.assertz = function(term)
{
    var functor = Utils.clause_functor(term);
    this.makeDynamic(functor);

    // If there are no clauses then assertz is the same as asserta, and the code in asserta is simpler
    if (this.predicates[functor].firstClause == null)
        this.asserta(term);

    var clause = Compiler.compilePredicateClause(term, true, this.predicates[functor].meta);
    this.predicates[functor].clauses.push(term);
    // We only get here if there are at least some clauses
    for (var c = this.predicates[functor].firstClause; c != null; c = c.nextClause)
    {
        if (c.nextClause == null)
        {
            c.nextClause = clause;
            break;
        }
    }
}

Module.prototype.predicateExists = function(functor)
{
    return this.predicates[functor] !== undefined;
}

Module.prototype.retractClause = function(functor, index)
{
    this.predicates[functor].clauses.splice(index, 1);
    //console.log("After retraction, the predicate has " + this.predicates[functor].clauses.length + " clauses left");
    // This is the trickiest operation
    if (index == 0)
        this.predicates[functor].firstClause = this.predicates[functor].firstClause.nextClause;
    else
    {
        var c = this.predicates[functor].firstClause;
        for (var i = 0; i < index-1; i++)
            c = c.nextClause;
        c.nextClause = c.nextClause.nextClause;
    }
}

Module.prototype.abolish = function(indicator)
{
    Utils.must_be_pi(indicator);
    var functor = Functor.get(ARGOF(indicator, 0), CTable.get(ARGOF(indicator, 1)).value);
    if (this.predicates[functor] === undefined || this.predicates[functor].dynamic !== true)
        Errors.permissionError(Constants.modifyAtom, Constants.staticProcedureAtom, indicator);
    this.predicates[functor] = undefined;
}

Module.prototype.addClause = function(functor, clause)
{
    // FIXME: Check that this is not an access violation (for example, writing to ','/2)
    if (functor == Constants.conjunctionFunctor)
        throw new Error("Cannot change ,/2: " + clause);
    if (this.predicates[functor] === undefined)
        this.definePredicate(functor);
    // FIXME: This should only really happen for static predicates
    this.predicates[functor].clauses.push(clause);
}

Module.prototype.compilePredicate = function(functor)
{
    this.predicates[functor].firstClause = Compiler.compilePredicate(this.predicates[functor].clauses, this.predicates[functor].meta);
}

Module.prototype.getPredicateCode = function(functor)
{
    //console.log("Looking for " + this.name + ":" + CTable.get(functor).toString());
    if (this.predicates[functor] === undefined)
    {
        return undefined;
    }
    if (this.predicates[functor].firstClause === null)
        this.compilePredicate(functor);
    //console.log(util.inspect(this.predicates[functor], {showHidden: false, depth: null}));
    return this.predicates[functor].firstClause;
}



module.exports = Module;
