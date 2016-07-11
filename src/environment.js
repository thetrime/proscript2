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
var Errors = require('./errors.js');
var Frame = require('./frame.js');
var Instructions = require('./opcodes.js').opcode_map;
var Compiler = require('./compiler.js');
var XMLHttpRequest = require('xmlhttprequest').XMLHttpRequest;
var IO = require('./io.js');
var Choicepoint = require('./choicepoint.js');
var fs = require('fs');
var BlobTerm = require('./blob_term');
var Term = require('./term');
var ArrayUtils = require('./array_utils');
var PrologFlag = require('./prolog_flag');


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
                      require('./iso_stream.js'),
                      require('./record.js'),
                      require('./foreign.js')];

var builtinModules = [fs.readFileSync(__dirname + '/builtin.pl', 'utf8')];

function Environment()
{
    this.userModule = Module.get("user");
    // We have to set currentModule here so that we can load the builtins. It will be reset in reset() again to user if it was changed
    this.currentModule = this.userModule;
    for (var i = 0; i < foreignModules.length; i++)
    {
        predicate_names = Object.keys(foreignModules[i]);
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
    for (var i = 0; i < builtinModules.length; i++)
    {
        this.consultString(builtinModules[i]);
    }
    this.reset();
}

Environment.prototype.create_choicepoint = function(data)
{
    this.choicepoints.push(new Choicepoint(this, 1));
    this.currentFrame.reserved_slots[0] = data;
    return this.choicepoints.length;
}

Environment.prototype.unify = function(a, b)
{
    a = a.dereference();
    b = b.dereference();
    //console.log("Unify: " + a + " vs " + b);
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
    //console.log("Binding " + util.inspect(variable) + " to " + util.inspect(value) + " at " + this.TR);
    this.trail[this.TR] = variable;
    variable.limit = this.TR++;
    variable.value = value;
}

Environment.prototype.halt = function(exitcode)
{
    this.halted = true;
    this.exitcode = exitcode;
}

Environment.prototype.reset = function()
{
    this.currentModule = this.userModule;
    this.moduleStack = [this.userModule];
    this.currentFrame = undefined;
    this.choicepoints = [];
    this.trail = [];
    this.TR = 0;
    this.currentModule = Module.get("user");
    this.argS = [];
    this.argI = 0;
    this.mode = 0; // READ
    this.trail = [];
    this.halted = false;
    this.exitcode = -1;
    var stdout = new Stream(null,
                            console_write,
                            null,
                            null,
                            null,
                            []);
    stdout.do_buffer = false;
    var null_stream = new Stream(null,
                                 null,
                                 null,
                                 null,
                                 null,
                                 []);

    this.streams = {current_input: null_stream,
                    current_output: stdout};
}

Environment.prototype.getModule = function()
{
    return this.currentModule;
}

Environment.prototype.consultString = function(data)
{
    var stream = new Stream(Stream.string_read,
                            null,
                            null,
                            null,
                            null,
                            IO.toByteArray(data));
    var clause = null;
    var module = this.currentModule;
    while ((clause = Parser.readTerm(stream, [])) != null)
    {
        //console.log("Read: " + clause);
        if (clause instanceof CompoundTerm && clause.functor.equals(Constants.directiveFunctor))
        {
            var directive = clause.args[0];
            if (directive instanceof CompoundTerm && directive.functor.equals(Constants.moduleFunctor))
            {
                Term.must_be_atom(directive.args[0]);
                // Compile the shims in user
                var exports = Term.to_list(directive.args[1]);
                for (var i = 0; i < exports.length; i++)
                {
                    Term.must_be_pi(exports[i]);
                    // For an export of foo/3, add a clause foo(A,B,C):- Module:foo(A,B,C).  to user
                    var functor = new Functor(exports[i].args[0], exports[i].args[1].value);
                    var args = new Array(exports[i].args[1].value);
                    for (var j = 0; j < exports[i].args[1].value; j++)
                        args[j] = new VariableTerm();
                    var head = new CompoundTerm(exports[i].args[0], args);
                    var shim = new CompoundTerm(Constants.clauseFunctor, [head, new CompoundTerm(Constants.crossModuleCallFunctor, [directive.args[0], head])]);
                    this.userModule.addClause(functor, shim);
                }
                this.currentModule = Module.get(directive.args[0].value)
            }
            // FIXME: Process any other special directives here!
            else
            {
                console.log("Processing directive " + directive);
                if (this.execute(directive))
                    console.log("    Directive succeeded");
                else
                    console.log("    Directive failed");
            }
        }
        else
        {
            var functor = clauseFunctor(clause);2
            this.getModule().addClause(functor, clause);
        }
    }
    // In case this has changed, set it back after consulting any file
    this.currentModule = this.userModule;
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
    for (var i = 0; i < queryCode.variables.length; i++)
        queryFrame.slots[i] = queryCode.variables[i];
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
        // Try again in module(user)
        p = this.userModule.getPredicateCode(functor);
    }
    if (p === undefined)
    {
        if (PrologFlag.values.unknown == "error")
            Errors.existenceError(Constants.procedureAtom, new CompoundTerm(Constants.predicateIndicatorFunctor, [functor.name, new IntegerTerm(functor.arity)]));
        else if (PrologFlag.values.unknown == "fail")
        {
            return Compiler.fail()
        }
        else
            Errors.existenceError(Constants.procedureAtom, new CompoundTerm(Constants.predicateIndicatorFunctor, [functor.name, new IntegerTerm(functor.arity)]));
    }

    return p;
}

Environment.prototype.backtrack = function()
{
    return Kernel.backtrack(this) && Kernel.execute(this);
}

Environment.prototype.copyTerm = function(t)
{
    t = t.dereference();
    var variables = [];
    Compiler.findVariables(t, variables);
    var newVars = new Array(variables.length);
    for (var i = 0; i < variables.length; i++)
        newVars[i] = new VariableTerm();
    return _copyTerm(t, variables, newVars);
}

function _copyTerm(t, vars, newVars)
{
    if (t instanceof VariableTerm)
    {
        return newVars[vars.indexOf(t)];
    }
    else if (t instanceof CompoundTerm)
    {
        var newArgs = new Array(t.args.length);
        for (var i = 0; i < t.args.length; i++)
            newArgs[i] = _copyTerm(t.args[i].dereference(), vars, newVars);
        return new CompoundTerm(t.functor, newArgs);
    }
    // For everything else, just return the term itself
    return t;
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
            _this.consultString(xhr.responseText);
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

Environment.prototype.consultFile = function(filename, callback)
{
    var _this = this;
    fs.readFile(filename, 'utf8', function(error, data)
                {
                    if (error) throw error;
                    _this.consultString(data);
                    callback();
                });
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

var console_buffer = '';
function console_write(stream, count, buffer)
{
    var str = console_buffer + fromByteArray(buffer);
    var lines = str.split('\n');
    for (var i = 0; i < lines.length-1; i++)
    {
        if (lines[i].length > 0)
            console.log(" ***************: " + lines[i]);
    }
    console_buffer = lines[lines.length-1];
    return count;
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

