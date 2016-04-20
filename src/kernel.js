var LOOKUP_OPCODE = require('./opcodes.js').opcodes;
var Functor = require('./functor.js');
var Frame = require('./frame.js');
var Module = require('./module.js');
var util = require('util');
var VariableTerm = require('./variable_term.js');
var AtomTerm = require('./atom_term.js');
var CompoundTerm = require('./compound_term.js');
var Choicepoint = require('./choicepoint.js');


// Binding is done in a slightly weird way
// To bind a variable to a value (which might also be a variable)
// we set two properties: First, .value is set to the value directly
// but also .limit is set to the current value of env.lTop
// This means we can undo bindings by simply descreasing env.lTop, provided
// that deref() respects the value of lTop
function deref(env, term)
{
    while(true)
    {
        if (term instanceof VariableTerm)
        {
            if (term.limit >= env.lTop)
            {
                // Reset the variable if the binding is above lTop
                term.value = null;
                return term;
            }
            else if (term.value == null)
            {
                // Already unbound
                return term
            }
            term = term.value;
            continue;
        }
        return term;
    }
}

function backtrack(env)
{
    if (env.choicepoints.length == 0)
        return false;
    var choicepoint = env.choicepoints.pop();
    env.currentFrame = choicepoint.frame;
    env.PC = choicepoint.retryPC;
    env.lTop = choicepoint.retrylTop;
    env.argP = choicepoint.argP;
    env.argI = choicepoint.argI;
    env.argS = choicepoint.argS;
    return true;
}

function bind(env, variable, value)
{
    variable.limit = env.lTop++;
    variable.value = value;
}

function execute(env)
{
    var currentModule = Module.get("user");
    env.argS = [];
    env.argI = 0;
    env.argP = env.currentFrame.slots;
    var nextFrame = undefined;
    while(true)
    {
        if (env.currentFrame.code[env.PC] === undefined)
        {
            console.log(util.inspect(env.currentFrame));
            console.log("Illegal fetch at " + env.PC);
            throw("Illegal fetch");
        }
        console.log(env.currentFrame.functor + " " + env.PC + ": " + LOOKUP_OPCODE[env.currentFrame.code[env.PC]].label);
        switch(LOOKUP_OPCODE[env.currentFrame.code[env.PC]].label)
	{
            case "i_fail":
            {
                if (backtrack(env))
                    continue;
                return false;
            }
            case "i_save_cut":
            {
                throw "not implemented";
                env.PC++;
                continue;
            }
            case "i_enter":
            {
                nextFrame = new Frame(env.currentFrame);
                env.argI = 0;
                env.argP = nextFrame.slots;
                env.PC++;
                continue;
            }
            case "i_exitquery":
            {
                return true;
            }
            case "i_exit":
            {
                env.PC = env.currentFrame.returnPC;
                env.currentFrame = env.currentFrame.parent;
                nextFrame = new Frame(env.currentFrame);
                env.argP = nextFrame.slots;
                env.argI = 0;
                continue;
            }
	    case "i_exitfact":
            {
                throw "not implemented";
            }
            case "i_depart":
            {
                var functor = Functor.lookup((env.currentFrame.code[env.PC+1] << 8) | (env.currentFrame.code[env.PC+2]));
                nextFrame.functor = functor;
                nextFrame.code = env.getPredicateCode(functor);
                nextFrame.PC = env.currentFrame.returnPC;
                nextFrame.parent = env.currentFrame.parent;
                env.argP = env.currentFrame.slots;
                env.argI = 0;
                env.currentFrame = nextFrame;
                nextFrame = new Frame(env.currentFrame);
                env.PC = 0; // Start from the beginning of the code in the next frame
                continue;

            }
            case "i_call":
            {
                var functor = Functor.lookup((env.currentFrame.code[env.PC+1] << 8) | (env.currentFrame.code[env.PC+2]));
                nextFrame.functor = functor;
                nextFrame.code = env.getPredicateCode(functor);
                nextFrame.returnPC = env.PC+3;
                env.currentFrame = nextFrame;
                env.argI = 0;
                env.argP = nextFrame.slots;
                nextFrame = new Frame(env.currentFrame);
                console.log(util.inspect(env.argP));
                console.log("Calling " + functor);
                env.PC = 0; // Start from the beginning of the code in the next frame
                continue;
            }
            case "i_cut":
            {
                throw "not implemented";
                env.PC++;
                continue;
            }
            case "i_unify":
            {
                throw "not implemented";
                env.PC++;
                continue;
            }
            case "b_firstvar":
            {
                var slot = ((env.currentFrame.code[env.PC+1] << 8) | (env.currentFrame.code[env.PC+2]));
                env.argP[env.argI++] = env.currentFrame.slots[slot];
                env.PC+=3;
                continue;
            }
            case "b_argvar":
            {
                var slot = ((env.currentFrame.code[env.PC+1] << 8) | (env.currentFrame.code[env.PC+2]));
                env.PC+=3;
                env.argP[env.argI++] = env.currentFrame.slots[slot]; // FIXME: Need to trail this?
                continue;
            }
            case "b_var":
            {
                throw "not implemented";
                env.PC+=3;
                continue;
            }
            case "b_pop":
            {
                throw "not implemented";
                env.PC++;
                continue;
            }
            case "b_atom":
            {
                throw "not implemented";
                env.PC+=3;
                continue;
            }
            case "b_functor":
            {
                throw "not implemented";
                env.PC+=3;
                continue;
            }
            case "h_firstvar":
            {
                throw "not implemented";
                env.PC+=3;
                continue;
            }
            case "h_functor":
            {
                var functor = Functor.lookup((env.currentFrame.code[env.PC+1] << 8) | (env.currentFrame.code[env.PC+2]));
                var arg = deref(env, env.argP[env.argI++]);
                env.PC+=3;
                if (arg instanceof CompoundTerm)
                {
                    if (arg.functor === functor)
                    {
                        env.argS.push({p: env.argP,
                                       i: env.argI});
                        env.argP = arg.args;
                        env.argI = 0;
                        continue;
                    }
                }
                else if (arg instanceof VariableTerm)
                {
                    env.argS.push({p: env.argP,
                                   i: env.argI});
                    var args = new Array(functor.arity);
                    for (var i = 0; i < args.length; i++)
                        args[i] = new VariableTerm("_");
                    bind(env, arg, new CompoundTerm(functor, args));
                    env.argP = args;
                    env.argI = 0;
                    continue;
                }
                console.log("argP: " + util.inspect(env.argP));
                console.log("Failed to unify " + util.inspect(arg) + " with the functor " + util.inspect(functor));

                if (backtrack(env))
                    continue;
                return false;
            }
            case "h_pop":
            {
                var argFrame = env.argS.pop();
                env.argP = argFrame.p;
                env.argI = argFrame.i;
                env.PC++;
                continue;
            }
            case "h_atom":
            {
                var atom = AtomTerm.lookup((env.currentFrame.code[env.PC+1] << 8) | (env.currentFrame.code[env.PC+2]));
                var arg = deref(env, env.argP[env.argI++]);
                env.PC+=3;
                if (arg == atom)
                    continue;
                if (arg instanceof VariableTerm)
                {
                    bind(env, arg, atom);
                }
                else
                {
                    console.log("argP: " + util.inspect(env.argP));
                    console.log("Failed to unify " + util.inspect(arg) + " with the atom " + util.inspect(atom));
                    if (backtrack(env))
                        continue;
                    return false;
                }
                continue;
            }
            case "h_void":
            {
	        env.PC++;
	        continue;
            }
	    case "h_var":
	    env.PC+=3;
	    continue;
            case "try_me_else":
            case "retry_me_else":
            {
                var address = (env.currentFrame.code[env.PC+1] << 24) | (env.currentFrame.code[env.PC+2] << 16) | (env.currentFrame.code[env.PC+3] << 8) | (env.currentFrame.code[env.PC+4] << 0) + env.PC;
                var backtrackFrame = new Choicepoint(env.currentFrame, address);
                backtrackFrame.retrylTop = env.lTop;
                backtrackFrame.PC = address;
                backtrackFrame.argP = env.argP;
                backtrackFrame.argI = env.argI;
                backtrackFrame.argS = env.argS;
                backtrackFrame.code = env.currentFrame.code;
                backtrackFrame.functor = env.currentFrame.functor;
                env.choicepoints.push(backtrackFrame);
                env.PC+=5;
                continue;
            }
            case "trust_me":
            {
                env.PC++;
                continue;
            }
            default:
            {
                console.log("Unknown instruction: " +LOOKUP_Oenv.PCODE[env.currentFrame.code[env.PC]].label);
                throw "illegal instruction";
            }
        }
    }
}

module.exports = {execute: execute};
