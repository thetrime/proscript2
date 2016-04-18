var Module = require('./module.js');
var Parser = require('./parser.js');
var util = require('util');
var Kernel = require('./kernel.js');
var AtomTerm = require('./atom_term.js');
var Constants = require('./constants.js');

function Environment()
{
    this.userModule = Module.get("user");
    this.currentModule = this.userModule;
}

Environment.prototype.getModule = function()
{
    return this.currentModule;
}

Environment.prototype.consultString = function(data)
{
    var clause = Parser.test(data);
    //console.log(util.inspect(clause, {showHidden: false, depth: null}));
    var functor = clauseFunctor(clause);
    this.getModule().addClause(functor, clause);
}

Environment.prototype.execute = function(queryCode)
{
    // make a frame with 0 args (since a query has no head)
    var queryFrame = {code:queryCode.bytecode,
                      slot: [],
                      parent: undefined,
                      PC: 0}
    Kernel.execute(this, queryFrame);
}

Environment.prototype.getPredicateCode = function(functor)
{
    var p = this.currentModule.getPredicateCode(functor);
    if (p === undefined && this.currentModule != this.userModule)
    {
        p = this.userModule.getPredicateCode(functor);
        if (p === undefined)
            throw "undefined predicate";
        // FIXME: Not so simple
    }
    return p;
}

function clauseFunctor(term)
{
    var head;
    if (term.functor === Constants.clauseFunctor)
    {
        head = term.args[0];
    }
    else
    {
        head = term;
    }
    if (head instanceof AtomTerm)
        return Functor.get(head, 0);
    else
        return head.functor;

}

module.exports = Environment;

