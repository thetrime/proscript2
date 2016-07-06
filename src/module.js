var module_map = [];
var Compiler = require('./compiler.js');
var Functor = require('./functor.js');
var AtomTerm = require('./atom_term.js');
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

Module.prototype.addClause = function(functor, clause)
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
    console.log("Found: " + util.inspect(this.predicates[functor.toString()]));
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
