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

var ForeignLib = require('./foreign.js');

function builtin(module, name)
{
    fn = module[name];
    if (fn === undefined)
        throw new Error("Could not find builtin " + name);
    return {name:name, arity:fn.length, fn:fn};
}

var builtins = [builtin(ForeignLib, "writeln"),
                builtin(ForeignLib, "fail"),
                builtin(ForeignLib, "true")];


function Environment()
{
    this.userModule = Module.get("user");
    for (var i = 0; i < builtins.length; i++)
    {
        this.userModule.defineForeignPredicate(builtins[i].name, builtins[i].arity, builtins[i].fn);
    }
    this.reset();
}

Environment.prototype.reset = function()
{
    this.currentModule = this.userModule;
    this.currentFrame = undefined;
    this.choicepoints = [];
    this.trail = [];
    this.TR = 0;
    this.currentModule = Module.get("user");
    this.argS = [];
    this.argI = 0;
    this.mode = 0; // READ
    this.trail = [];
    var stdout = Stream.new_stream(null,
                                   console_write,
                                   null,
                                   null,
                                   null,
                                   []);
    this.streams = {stdin: undefined,
                    stdout: stdout,
                    stderr: stdout};
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
    var topFrame = new Frame(this);
    topFrame.functor = new Functor(new AtomTerm("$top"), 0);
    topFrame.code = {opcodes: [Instructions.iExitQuery.opcode],
		     constants: []};
    this.currentFrame = topFrame;
    var queryFrame = new Frame(this);
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

function fromByteArray(byteArray)
{
    var str = '';
    for (i = 0; i < byteArray.length; i++)
    {
        if (byteArray[i] <= 0x7F)
        {
            str += String.fromCharCode(byteArray[i]);
        }
        else
        {
            // Have to decode manually
            var ch = 0;
            var mask = 0x20;
            var j = 0;
            for (var mask = 0x20; mask != 0; mask >>=1 )
            {        
                var next = byteArray[j+1];
                if (next == undefined)
                {
                    abort("Unicode break in fromByteArray. The input is garbage");
                }
                ch = (ch << 6) ^ (next & 0x3f);
                if ((byteArray[i] & mask) == 0)
                    break;
                j++;
            }
            ch ^= (b & (0xff >> (i+3))) << (6*(i+1));
            str += String.fromCharCode(ch);
        }
    }
    return str;
}

function console_write(stream, size, count, buffer)
{
    var str = fromByteArray(buffer);
    console.log(str);
    return size*count;
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

