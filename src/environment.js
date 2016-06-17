var Module = require('./module.js');
var Parser = require('./parser.js');
var util = require('util');
var Kernel = require('./kernel.js');
var AtomTerm = require('./atom_term.js');
var Functor = require('./functor.js');
var Constants = require('./constants.js');
var Stream = require('./stream.js');
var Frame = require('./frame.js');
var Instructions = require('./opcodes.js').opcode_map;
var Compiler = require('./compiler.js');
var XMLHttpRequest = require('xmlhttprequest').XMLHttpRequest;
var IO = require('./io.js');

function Environment()
{
    this.userModule = Module.get("user");
    this.reset();
}

Environment.prototype.reset = function()
{
    this.currentModule = this.userModule;
    this.choicepoints = [];
    this.trail = [];
    this.lTop = 0;
    this.currentModule = Module.get("user");
    this.argS = [];
    this.argI = 0;
    this.mode = 0; // READ

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
    topFrame.code = {opcodes: [Instructions.iExitQuery.opcode],
		     constants: []};

    var queryFrame = new Frame(topFrame);
    queryFrame.functor = "$query";
    queryFrame.code = {opcodes: queryCode.bytecode,
		       constants: queryCode.constants};
    queryFrame.slots = [queryTerm.args[0]];
    queryFrame.returnPC = 0;
    this.PC = 0;
    this.currentFrame = queryFrame;
    this.argP = queryFrame.slots;
    return Kernel.execute(this);
}

Environment.prototype.getPredicateCode = function(functor)
{
    var p = this.currentModule.getPredicateCode(functor);
    if (p === undefined && this.currentModule != this.userModule)
    {
        p = this.userModule.getPredicateCode(functor);
        if (p === undefined)
            throw "undefined predicate";
    }
    return p;
}

Environment.prototype.backtrack = function()
{
    return Kernel.backtrack(this) && Kernel.execute(this);
}

Environment.prototype.consultURL = function(url, callback)
{
    var xhr = new XMLHttpRequest();
    xhr.open('get', url, true);
    var _this = this;
    xhr.onload = function()
    {
	var status = xhr.status;
	if (status == 200)
	{
	    var stream = Stream.new_stream(xhr_read,
					   null,
					   null,
					   null,
					   null,
					   IO.toByteArray(xhr.responseText));
	    var clause = null;
	    while ((clause = Parser.readTerm(stream, [])) != null)
	    {
		console.log("Clause: " + clause);
		var functor = clauseFunctor(clause);
		_this.getModule().addClause(functor, clause);
	    }
	    callback();
	}
	else
	{
	    // FIXME: Do something better here?
	    console.log("Failed to consult " + url);
	    console.log(status);
	}
    };
    xhr.send();
}

function xhr_read(stream, size, count, buffer)
{
    var bytes_read = 0;
    var records_read;
    for (records_read = 0; records_read < count; records_read++)
    {
        for (var b = 0; b < size; b++)
        {
            t = stream.data.shift();
            if (t === undefined)
            {                
                return records_read;
            }
            buffer[bytes_read++] = t;
        }
    }
    return records_read;
}

function clauseFunctor(term)
{
    if (term instanceof AtomTerm)
	return new Functor(term, 0);
    if (term.functor.equals(Constants.clauseFunctor))
	return clauseFunctor(term.args[0]);
    return term.functor;
}

module.exports = Environment;

