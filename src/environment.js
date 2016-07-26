"use strict";
exports=module.exports;

var Module = require('./module.js');
var Parser = require('./parser.js');
var Kernel = require('./kernel.js');
var AtomTerm = require('./atom_term.js');
var FloatTerm = require('./float_term.js');
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
var Choicepoint = require('./choicepoint.js');
var fs = require('fs');
var Utils = require('./utils');
var PrologFlag = require('./prolog_flag');
var Clause = require('./clause');
var CTable = require('./ctable');
var util = require('util');


function builtin(module, name)
{
    fn = module[name];
    if (fn === undefined)
        throw new Error("Could not find builtin " + name);
    return {name:name, arity:fn.length, fn:fn};
}

var foreignModules = [require('./iso_foreign.js'),
                      require('./iso_arithmetic.js'),
                      require('./iso_stream.js'),
                      require('./record.js'),
                      require('./foreign.js')];

var builtinModules = [fs.readFileSync(__dirname + '/builtin.pl', 'utf8')];

function Environment()
{

    this.module_map = [];

    this.debug_ops = {};
    this.debug_times = {};
    this.debug_backtracks = 0;
    this.debugger_steps = 0;
    this.debugging = true;
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

Environment.prototype.yield_control = function()
{
    var that = this;
    return function(success) { Kernel.resume(that, success)};
}

Environment.prototype.create_choicepoint = function(data, cleanup)
{
    // FIXME: If this is a foreign meta-predicate then 1 is not correct here! Instead maybe this.PC+1?
    var c = new Choicepoint(this, 1);
    if (cleanup != undefined)
        c.cleanup = {foreign: cleanup};
    this.choicepoints[this.CP++] = c;
    this.currentFrame.reserved_slots[0] = data;
    return this.CP;
}

Environment.prototype.saveState = function()
{
    var saved = {currentModule: this.currentModule,
                 currentFrame: this.currentFrame,
                 choicepoints: this.choicepoints,
                 TR: this.TR,
                 trail: this.trail,
                 PC: this.PC,
                 stream: this.streams,
                 yieldInfo: this.yieldInfo,
                 // In theory, argS, argP and argI need not be saved.
                 // argS SHOULD always be [] at a save point, and argP
                 // can always be restored to this.currentFrame.slots,
                 // and argI to 0. Mode should also always be restored
                 // to ... write?
                 argS: this.argS,
                 argSI: this.argSI,
                 CP: this.CP,
                 argP: this.argP,
                 argI: this.argI,
                 mode: this.mode};
    this.reset();
    return saved;
}

Environment.prototype.restoreState = function(saved)
{
    this.currentModule = saved.currentModule;
    this.currentFrame = saved.currentFrame;
    this.choicepoints = saved.choicepoints;
    this.TR = saved.TR;
    this.argSI = saved.argSI;
    this.CP = saved.CP;
    this.argP = saved.argP;
    this.argI = saved.argI;
    this.trail = saved.trail;
    this.PC = saved.PC;
    this.mode = saved.mode;
    this.streams = saved.stream;
    this.yieldInfo = saved.yieldInfo;
}

Environment.prototype.unify = function(a, b)
{
    a = DEREF(a);
    b = DEREF(b);
    //console.log("Unify: " + a + " vs " + b);
    if (a == b)
        return true;
    if (a >>> 30 == 0) // A is a variable
    {
        this.bind(a, b);
        return true;
    }
    if (b >>> 30 == 0) // B is a variable
    {
        this.bind(b, a);
        return true;
    }
    if (a >>> 30 == 2 && b >>> 30 == 2 && FUNCTOROF(a) == FUNCTOROF(b))
    {
        var argcount = CTable.get(FUNCTOROF(a)).arity;
        for (var i = 0; i < argcount; i++)
	{
            if (!this.unify(ARGOF(a, i), ARGOF(b, i)))
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
    this.TR++;
    HEAP[variable] = value;
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
    this.argSI = 0;
    this.CP = 0;
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
                            Stream.stringBuffer(data));
    var clause = null;
    while ((clause = Parser.readTerm(stream, [])) != Constants.endOfFileAtom)
    {
        //console.log("Clause: " + PORTRAY(clause));
        if (TAGOF(clause) == CompoundTag && FUNCTOROF(clause) == Constants.directiveFunctor)
        {
            var directive = ARGOF(clause, 0);
            if (TAGOF(directive) == CompoundTag && FUNCTOROF(directive) == Constants.moduleFunctor)
            {
                Utils.must_be_atom(ARGOF(directive, 0))
                // Compile the shims in user
                var exports = Utils.to_list(ARGOF(directive, 1));
                for (var i = 0; i < exports.length; i++)
                {
                    Utils.must_be_pi(exports[i]);
                    // For an export of foo/3, add a clause foo(A,B,C):- Module:foo(A,B,C).  to user
                    var arity = CTable.get(ARGOF(exports[i], 1)).value;
                    var functor = Functor.get(ARGOF(exports[i], 0), arity);
                    var args = new Array(arity);
                    for (var j = 0; j < arity; j++)
                        args[j] = MAKEVAR();
                    var head = CompoundTerm.create(ARGOF(exports[i], 0), args);
                    var shim = CompoundTerm.create(Constants.clauseFunctor, [head, CompoundTerm.create(Constants.crossModuleCallFunctor, [ARGOF(directive, 0), head])]);
                    this.userModule.addClause(functor, shim);
                }
                this.currentModule = this.getModule(CTable.get(ARGOF(directive, 0)).value)
            }
            else if (TAGOF(directive) == CompoundTag && FUNCTOROF(directive) == Constants.metaPredicateFunctor)
            {
                Utils.must_be_compound(ARGOF(directive,0))
                var functor = FUNCTOROF(ARGOF(directive, 0));
                var args = new Array(CTable.get(functor).arity);
                for (var i = 0; i < args.length; i++)
                {
                    var arg = ARGOF(ARGOF(directive, 0),i);
                    var arg_object = CTable.get(arg);
                    if (arg_object instanceof AtomTerm)
                        args[i] = arg_object.value;
                    else if (arg_object instanceof IntegerTerm)
                        args[i] = arg_object.value;
                    else
                        Errors.typeError(Constants.metaArgumentSpecifierAtom, arg);
                }
                this.currentModule.makeMeta(functor, args);
            }
            else if (TAGOF(directive) == CompoundTag && FUNCTOROF(directive) == Constants.dynamicFunctor)
            {
                Utils.must_be_pi(ARGOF(directive,0));
                var functor = Functor.get(ARGOF(ARGOF(directive,0),0), CTable.get(ARGOF(ARGOF(directive,0),1)).value);
                this.currentModule.makeDynamic(functor);
            }
            else if (TAGOF(directive) == CompoundTag && FUNCTOROF(directive) == Constants.multiFileFunctor)
            {
                // FIXME: implement
            }
            else if (TAGOF(directive) == CompoundTag && FUNCTOROF(directive) == Constants.discontiguousFunctor)
            {
                // FIXME: implement
            }
            else if (TAGOF(directive) == CompoundTag && FUNCTOROF(directive) == Constants.initializationFunctor)
            {
                // FIXME: implement
            }
            // char_conversion/2, op/3 and set_prolog_flag/2, include/1 and ensure_loaded/1 are just executed, along with any other directives
            else
            {
                // FIXME: in /theory/ this could block and we should not assume that just because this.execute() has returned that the goal has completed.
                //        In /practise/ if a directive executes a blocking predicate, then it is probably a terrible directive.
                console.log("Processing directive " + PORTRAY(directive));
                this.execute(directive,
                             function(){console.log("    Directive succeeded");},
                             function(){console.log("    Directive failed");},
                             function(error){console.log("    Directive raised an exception: " + error.toString());});
            }
        }
        else
        {
            //console.log("Got ordinary clause: " + PORTRAY(clause));
            var functor = clauseFunctor(clause);
            this.currentModule.addClause(functor, clause);
        }
    }
    // In case this has changed, set it back after consulting any file
    this.currentModule = this.userModule;
}

Environment.prototype.execute = function(queryTerm, onSuccess, onFailure, onError)
{
    var compiledQuery = Compiler.compileQuery(queryTerm);
    // make a frame with 0 args (since a query has no head)
    var topFrame = new Frame(this);
    topFrame.functor = Functor.get(AtomTerm.get("$top"), 0);
    topFrame.clause = new Clause([Instructions.i_exitquery.opcode], [], ["i_exit_query"]);
    this.currentFrame = topFrame;
    var queryFrame = new Frame(this);
    queryFrame.functor = Functor.get(AtomTerm.get("$query"), 0);
    queryFrame.clause = compiledQuery.clause;
    for (var i = 0; i < compiledQuery.variables.length; i++)
        queryFrame.slots[i] = compiledQuery.variables[i];
    queryFrame.returnPC = 0;
    this.PC = 0;
    this.currentFrame = queryFrame;
    this.argP = queryFrame.slots;
    //console.log(queryTerm.toString());
    Kernel.execute(this, onSuccess, onFailure, onError);
}

Environment.prototype.getPredicateCode = function(functor, optionalContextModule)
{
    //console.log("Looking for " + CTable.get(functor).toString() + "(" + functor + ") in " + this.currentModule.name);
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
        var functor_object = CTable.get(functor);
        if (PrologFlag.values.unknown == "error")
            Errors.existenceError(Constants.procedureAtom, CompoundTerm.create(Constants.predicateIndicatorFunctor, [functor_object.name, IntegerTerm.get(functor_object.arity)]));
        else if (PrologFlag.values.unknown == "fail")
            return Compiler.fail()
        else if (PrologFlag.values.unknown == "warning")
        {
            //FIXME: warning("No such predicate " + functor);
            return Compiler.fail()
        }
        else
            Errors.existenceError(Constants.procedureAtom, CompoundTerm.create(Constants.predicateIndicatorFunctor, [functor_object.name, IntegerTerm.get(functor_object.arity)]));
    }

    return p;
}

Environment.prototype.backtrack = function(onSuccess, onFailure, onError)
{
    console.log("Backtracking...");
    if (Kernel.backtrack(this))
        Kernel.execute(this, onSuccess, onFailure, onError);
    else
        onFailure();
}

Environment.prototype.copyTerm = function(t)
{
    if (t.stack != undefined) console.log(t.stack);
    t = DEREF(t);
    var variables = [];
    //console.log("Copying: " + PORTRAY(t));
    Compiler.findVariables(t, variables);
    //console.log("Vars: " + variables);
    var newVars = new Array(variables.length);
    for (var i = 0; i < variables.length; i++)
        newVars[i] = MAKEVAR();
    return _copyTerm(t, variables, newVars);
}

function _copyTerm(t, vars, newVars)
{
    if (TAGOF(t) == VariableTag)
    {
        return newVars[vars.indexOf(t)];
    }
    else if (TAGOF(t) == CompoundTag)
    {
        var functor = CTable.get(FUNCTOROF(t));
        var newArgs = new Array(functor.arity);
        for (var i = 0; i < newArgs.length; i++)
            newArgs[i] = _copyTerm(ARGOF(t, i), vars, newVars);
        if (FUNCTOROF(t) == undefined)
            console.log("oops: " + HTOP);
        return CompoundTerm.create(FUNCTOROF(t), newArgs);
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


function fromByteArray(buffer, offset, length)
{
    var str = '';
    for (var i = offset; i < length; i++)
    {
        if (buffer.readUInt8(i) <= 0x7F)
        {
            str += String.fromCharCode(buffer.readUInt8(i));
        }
        else
        {
            // Have to decode manually
            var ch = 0;
            var mask = 0x20;
            var j = 0;
            for (var mask = 0x20; mask != 0; mask >>=1 )
            {        
                var next = buffer.readUInt8(j+1);
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
function console_write(stream, offset, length, buffer)
{
    var str = console_buffer + fromByteArray(buffer, offset, length);
    var lines = str.split('\n');
    for (var i = 0; i < lines.length-1; i++)
    {
        if (lines[i].length > 0)
            console.log(" ***************: " + lines[i]);
    }
    console_buffer = lines[lines.length-1];
    return length;
}

function clauseFunctor(term)
{
    Utils.must_be_bound(term);
    if (TAGOF(term) == ConstantTag && CTable.get(term) instanceof AtomTerm)
        return Functor.get(term, 0);
    if (FUNCTOROF(term) == Constants.clauseFunctor)
        return clauseFunctor(ARGOF(term,0));
    return FUNCTOROF(term);
}

module.exports = Environment;

