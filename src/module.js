var module_map = [];
var Compiler = require('./compiler.js');

function Module(name)
{
    this.name = name;
    this.predicates = {};
}

Module.prototype.definePredicate = function(functor)
{
    this.predicates[functor.index] = {clauses: [],
                                      code: undefined};
}

Module.prototype.addClause = function(functor, clause)
{
    this.predicates[functor.index].clauses.push(clause);
}

Module.prototype.compilePredicate = function(functor)
{
    this.predicates[functor.index].code = Compiler.compilePredicate(this.predicates[functor.index].clauses);
}

Module.prototype.getPredicate = function(functor)
{
    return this.predicates[functor.index].code;
}

Module.get = function(name)
{
    if (module_map[name] === undefined)
        module_map[name] = new Module(name);
    return module_map[name];
}

module.exports = Module;
