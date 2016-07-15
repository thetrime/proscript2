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
var Utils = require('./utils');
var ArrayUtils = require('./array_utils');
var PrologFlag = require('./prolog_flag');
var Clause = require('./clause');


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
    this.module_map = [];
    this.savedContexts = [];
    this.userModule = this.getModule("user");
    // We have to set currentModule here so that we can load the builtins. It will be reset in reset() again to user if it was changed
    this.currentModule = this.userModule;
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
    for (var i = 0; i < builtinModules.length; i++)
    {
        this.consultString(builtinModules[i]);
    }
    this.stdout = new Stream(null,
                            console_write,
                            null,
                            null,
                            null,
                            []);
    this.stdout.do_buffer = false;
    this.stdin = new Stream(null,
                            null,
                            null,
                            null,
                            null,
                            []);

    this.reset();
}

Environment.prototype.create_choicepoint = function(data)
{
    // FIXME: If this is a foreign meta-predicate then 1 is not correct here!
    this.choicepoints.push(new Choicepoint(this, 1));
    this.currentFrame.reserved_slots[0] = data;
    return this.choicepoints.length;
}

Environment.prototype.pushContext = function()
{
    this.savedContexts.push({currentModule: this.currentModule,
                             currentFrame: this.currentFrame,
                             choicepoints: this.choicepoints,
                             TR: this.TR,
                             argS: this.argS,
                             argP: this.argP,
                             argI: this.argI,
                             trail: this.trail,
                             PC: this.PC,
                             mode: this.mode,
                             stream: this.streams});
    this.reset();
}

Environment.prototype.popContext = function()
{
    var saved = this.savedContexts.pop();
    this.currentModule = saved.currentModule;
    this.currentFrame = saved.currentFrame;
    this.choicepoints = saved.choicepoints;
    this.TR = saved.TR;
    this.argS = saved.argS;
    this.argP = saved.argP;
    this.argI = saved.argI;
    this.trail = saved.trail;
    this.PC = saved.PC;
    this.mode = saved.mode;
    this.streams = saved.stream;
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
    this.currentFrame = undefined;
    this.choicepoints = [];
    this.TR = 0;
    this.argS = [];
    this.argI = 0;
    this.mode = "read";
    this.trail = [];
    this.halted = false;
    this.exitcode = -1;
    this.streams = {current_input: this.stdin,
                    current_output: this.stdout};
}

Environment.prototype.predicateExists = function(module, functor)
{
    if (this.module_map[module] === undefined)
        return false;
    return this.module_map[module].predicateExists(functor);
}

Environment.prototype.getModule = function(name)
{
    if (this.module_map[name] === undefined)
        this.module_map[name] = new Module(name);
    return this.module_map[name];
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
    while (!((clause = Parser.readTerm(stream, [])).equals(Constants.endOfFileAtom)))
    {
        //console.log("Read: " + clause);
        if (clause instanceof CompoundTerm && clause.functor.equals(Constants.directiveFunctor))
        {
            var directive = clause.args[0];
            if (directive instanceof CompoundTerm && directive.functor.equals(Constants.moduleFunctor))
            {
                Utils.must_be_atom(directive.args[0]);
                // Compile the shims in user
                var exports = Utils.to_list(directive.args[1]);
                for (var i = 0; i < exports.length; i++)
                {
                    Utils.must_be_pi(exports[i]);
                    // For an export of foo/3, add a clause foo(A,B,C):- Module:foo(A,B,C).  to user
                    var functor = new Functor(exports[i].args[0], exports[i].args[1].value);
                    var args = new Array(exports[i].args[1].value);
                    for (var j = 0; j < exports[i].args[1].value; j++)
                        args[j] = new VariableTerm();
                    var head = new CompoundTerm(exports[i].args[0], args);
                    var shim = new CompoundTerm(Constants.clauseFunctor, [head, new CompoundTerm(Constants.crossModuleCallFunctor, [directive.args[0], head])]);
                    this.userModule.addClause(functor, shim);
                }
                this.currentModule = this.getModule(directive.args[0].value)
            }
            else if (directive instanceof CompoundTerm && directive.functor.equals(Constants.metaPredicateFunctor))
            {
                Utils.must_be_compound(directive.args[0]);
                var functor = directive.args[0].functor;
                var args = new Array(directive.args[0].functor.arity);
                for (var i = 0; i < directive.args[0].functor.arity; i++)
                {
                    if (directive.args[0].args[i] instanceof AtomTerm)
                        args[i] = directive.args[0].args[i].value;
                    else if (directive.args[0].args[i] instanceof IntegerTerm)
                        args[i] = directive.args[0].args[i].value;
                    else
                        Errors.typeError(Constants.metaArgumentSpecifierAtom, directive.args[0].args[i]);
                }
                this.currentModule.makeMeta(functor, args);
            }
            else if (directive instanceof CompoundTerm && directive.functor.equals(Constants.dynamicFunctor))
            {
                Utils.must_be_pi(directive.args[0]);
                var functor = new Functor(directive.args[0].args[0], directive.args[0].args[1].value);
                this.currentModule.makeDynamic(functor);
            }
            else if (directive instanceof CompoundTerm && directive.functor.equals(Constants.multiFileFunctor))
            {
                // FIXME: implement
            }
            else if (directive instanceof CompoundTerm && directive.functor.equals(Constants.discontiguousFunctor))
            {
                // FIXME: implement
            }
            else if (directive instanceof CompoundTerm && directive.functor.equals(Constants.initializationFunctor))
            {
                // FIXME: implement
            }
            // char_conversion/2, op/3 and set_prolog_flag/2, include/1 and ensure_loaded/1 are just executed, along with any other directives
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
            var functor = clauseFunctor(clause);
            this.currentModule.addClause(functor, clause);
        }
    }
    // In case this has changed, set it back after consulting any file
    this.currentModule = this.userModule;
}

Environment.prototype.execute = function(queryTerm)
{
    var compiledQuery = Compiler.compileQuery(queryTerm);

    // make a frame with 0 args (since a query has no head)
    var topFrame = new Frame(this);
    topFrame.functor = new Functor(new AtomTerm("$top"), 0);
    topFrame.clause = new Clause([Instructions.iExitQuery.opcode], [], ["i_exit_query"]);
    this.currentFrame = topFrame;
    var queryFrame = new Frame(this);
    queryFrame.functor = new Functor(new AtomTerm("$query"), 0);
    queryFrame.clause = compiledQuery.clause;
    for (var i = 0; i < compiledQuery.variables.length; i++)
        queryFrame.slots[i] = compiledQuery.variables[i];
    queryFrame.returnPC = 0;
    this.PC = 0;
    this.currentFrame = queryFrame;
    this.argP = queryFrame.slots;
    return Kernel.execute(this);
}

Environment.prototype.getPredicateCode = function(functor, optionalContextModule)
{
    //console.log("Looking for " + functor + " in " + this.currentModule.name);
    var m = optionalContextModule || this.currentModule;
    var p = m.getPredicateCode(functor);
    if (p === undefined && m != this.userModule)
    {
        // Try again in module(user)
        p = this.userModule.getPredicateCode(functor);
        if (p != undefined)
        {
            // Force a context switch to user
            this.currentModule = this.userModule;
            p.module = this.userModule;
        }
    }
    else if (p !== undefined)
        p.module = m;
    if (p === undefined)
    {
        if (PrologFlag.values.unknown == "error")
            Errors.existenceError(Constants.procedureAtom, new CompoundTerm(Constants.predicateIndicatorFunctor, [functor.name, new IntegerTerm(functor.arity)]));
        else if (PrologFlag.values.unknown == "fail")
            return Compiler.fail()
        else if (PrologFlag.values.unknown == "warning")
        {
            //FIXME: warning("No such predicate " + functor);
            return Compiler.fail()
        }
        else
            Errors.existenceError(Constants.procedureAtom, new CompoundTerm(Constants.predicateIndicatorFunctor, [functor.name, new IntegerTerm(functor.arity)]));
    }

    return p;
}

Environment.prototype.backtrack = function()
{
    console.log("Backtracking...");
    return Kernel.backtrack(this) && Kernel.execute(this);
}

Environment.prototype.copyTerm = function(t)
{
    if (t.stack != undefined) console.log(t.stack);
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
            ch ^= (byteArray[i] & (0xff >> (i+3))) << (6*(i+1));
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
    Utils.must_be_bound(term);
    if (term instanceof AtomTerm)
        return new Functor(term, 0);
    if (term.functor.equals(Constants.clauseFunctor))
	return clauseFunctor(term.args[0]);
    return term.functor;
}

module.exports = Environment;

