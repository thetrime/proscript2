var Module = require('./module.js');
var Parser = require('./parser.js');
var util = require('util');
var Kernel = require('./kernel.js');
var AtomTerm = require('./atom_term.js');
var VariableTerm = require('./variable_term.js');
var CompoundTerm = require('./compound_term.js');
var IntegerTerm = require('./integer_term.js');
var Functor = require('./functor.js');
var Constants = require('./constants.js');
var Stream = require('./stream.js');
var Frame = require('./frame.js');
var Instructions = require('./opcodes.js').opcode_map;
var Compiler = require('./compiler.js');
var XMLHttpRequest = require('xmlhttprequest').XMLHttpRequest;
var IO = require('./io.js');

function builtin(module, name)
{
    fn = module[name];
    if (fn === undefined)
        throw new Error("Could not find builtin " + name);
    return {name:name, arity:fn.length, fn:fn};
}

var foreignModules = [require('./iso_foreign.js'),
                      require('./iso_arithmetic.js'),
                      require('./iso_record.js'),
                      require('./foreign.js')];


function Environment()
{
    this.userModule = Module.get("user");
    for (var i = 0; i < foreignModules.length; i++)
    {
        var predicate_names = Object.keys(foreignModules[i]);
        for (var p = 0; p < predicate_names.length; p++)
        {
            // Each export may either a be a function OR a list of functions
            // This is to accommodate things like halt(0) and halt(1), which would otherwise both be in the .halt slot
            var pred = foreignModules[i][predicate_names[p]];
            if (pred.constructor === Array)
            {
                for (var q = 0; q < pred.length; q++)
                {
                    this.userModule.defineForeignPredicate(predicate_names[p], pred[q]);
                }
            }
            else
                this.userModule.defineForeignPredicate(predicate_names[p], pred);
        }
    }
    this.reset();
}

Environment.prototype.create_choicepoint = function()
{
    // FIXME: Implement. This is not that trivial :(
}

Environment.prototype.unify = function(a, b)
{
    a = a.dereference();
    b = b.dereference();
    console.log("Unify: " + a + " vs " + b);
    if (a.equals(b))
        return true;
    if (a instanceof VariableTerm)
    {
        this.bind(a, b);
        return true;
    }
    if (b instanceof VariableTerm)
    {
        this.bind(b, a);
        return true;
    }
    if (a instanceof CompoundTerm && b instanceof CompoundTerm && a.functor.equals(b.functor))
    {
        for (var i = 0; i < a.args.length; i++)
	{
            if (!this.unify(a.args[i], b.args[i]))
                return false;
        }
	return true;
    }
    return false;
}

Environment.prototype.bind = function(variable, value)
{
    console.log("Binding " + util.inspect(variable) + " to " + util.inspect(value) + " at " + this.TR);
    this.trail[this.TR] = variable;
    variable.limit = this.TR++;
    variable.value = value;
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

