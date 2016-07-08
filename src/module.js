var module_map = [];
var Compiler = require('./compiler.js');
var Functor = require('./functor.js');
var AtomTerm = require('./atom_term.js');
var Term = require('./term.js');
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
    this.predicates[functor.toString()] = {code: {opcodes: compiled.bytecode,
                                                  constants: compiled.constants},
                                           functor: functor,
                                           instructions: compiled.instructions};
    console.log(">>> Defined (foreign) " + this.name + ":" + functor);
}

Module.prototype.definePredicate = function(functor)
{
    this.predicates[functor.toString()] = {clauses: [],
                                           functor: functor,
                                           code: undefined};
    console.log(">>> Defined " + this.name + ":" + functor);
}

Module.prototype.makeDynamic = function(functor)
{
    // FIXME: Do not allow adding clauses to compiled predicates?
    if (this.predicates[functor.toString()] === undefined)
        this.definePredicate(functor);
    this.predicates[functor.toString()].dynamic = true;
    this.predicates[functor.toString()].code = undefined;
}

Module.prototype.asserta = function(term)
{
    var functor = Term.clause_functor(term);
    this.makeDynamic(functor);
    this.predicates[functor.toString()].clauses.unshift(term);
}

Module.prototype.assertz = function(term)
{
    var functor = Term.clause_functor(term);
    this.makeDynamic(functor);
    this.predicates[functor.toString()].clauses.push(term);
}

Module.prototype.retractClause = function(functor, index)
{
    this.predicates[functor.toString()].clauses.splice(index, 1);
    // We must recompile the predicate on next access
    this.predicates[functor.toString()].code = undefined;
}

Module.prototype.abolish = function(indicator)
{
    Term.must_be_pi(indicator);
    var functor = new Functor(indicator.args[0], indicator.args[1].value);
    if (this.predicates[functor.toString()].dynamic !== true)
        Errors.permissionError(Constants.accessAtom, Constants.staticProcedureAtom, term);
    this.predicates[functor.toString()] = undefined;
}

Module.prototype.addClause = function(functor, clause)
{
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
    var compiled = Compiler.compilePredicate(this.predicates[functor.toString()].clauses);
    this.predicates[functor.toString()].code = {opcodes: compiled.bytecode,
						constants: compiled.constants};
    this.predicates[functor.toString()].instructions = compiled.instructions;
}

Module.prototype.getPredicateCode = function(functor)
{
    console.log("Looking for: " + functor);
    if (this.predicates[functor.toString()] === undefined)
    {
        console.log("No such predicate in module " + this.name);
        // FIXME: Handle this error properly
        throw(0);
        return undefined;
    }
    //console.log("Found: " + util.inspect(this.predicates[functor.toString()]));
    if (this.predicates[functor.toString()].code === undefined)
        this.compilePredicate(functor);
    //console.log(util.inspect(this.predicates[functor.toString()], {showHidden: false, depth: null}));
    return this.predicates[functor.toString()].code;
}

Module.get = function(name)
{
    if (module_map[name] === undefined)
        module_map[name] = new Module(name);
    return module_map[name];
}

module.exports = Module;
