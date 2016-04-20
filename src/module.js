var module_map = [];
var Compiler = require('./compiler.js');
var util = require('util');

function Module(name)
{
    this.name = name;
    this.predicates = {};
}

Module.prototype.definePredicate = function(functor)
{
    this.predicates[functor.index] = {clauses: [],
                                      code: undefined};
    console.log(">>> Defined " + this.name + ":" + functor);
}

Module.prototype.addClause = function(functor, clause)
{
    if (this.predicates[functor.index] === undefined)
        this.definePredicate(functor);
    this.predicates[functor.index].clauses.push(clause);
}

Module.prototype.compilePredicate = function(functor)
{
    var compiled = Compiler.compilePredicate(this.predicates[functor.index].clauses);
    this.predicates[functor.index].code = compiled.bytecode;
    this.predicates[functor.index].instructions = compiled.instructions;
}

Module.prototype.getPredicateCode = function(functor)
{
    if (this.predicates[functor.index] === undefined)
    {
        console.log("No such predicate in module " + this.name);
        return undefined;
    }
    if (this.predicates[functor.index].code === undefined)
        this.compilePredicate(functor);
    return this.predicates[functor.index].code;
}

Module.get = function(name)
{
    if (module_map[name] === undefined)
        module_map[name] = new Module(name);
    return module_map[name];
}

module.exports = Module;
