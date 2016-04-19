var Module = require('./module.js');
var Parser = require('./parser.js');
var util = require('util');
var Kernel = require('./kernel.js');
var AtomTerm = require('./atom_term.js');
var Constants = require('./constants.js');
var Frame = require('./frame.js');
var Instructions = require('./opcodes.js').opcode_map;
var Compiler = require('./compiler.js');

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

Environment.prototype.execute = function(queryTerm)
{
    var queryCode = Compiler.compileQuery(queryTerm);

    // make a frame with 0 args (since a query has no head)
    var topFrame = new Frame("$top");
    topFrame.functor = "$top";
    topFrame.code = [Instructions.iExitQuery.opcode];

    var queryFrame = new Frame(topFrame);
    queryFrame.functor = "$query";
    queryFrame.code = queryCode.bytecode;
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

