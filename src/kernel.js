var LOOKUP_OPCODE = require('./opcodes.js').opcodes;
var Functor = require('./functor.js');
var Frame = require('./frame.js');
var Module = require('./module.js');
var util = require('util');
var VariableTerm = require('./variable_term.js');
var AtomTerm = require('./atom_term.js');
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
    var argP = 0;
    var nextFrame = new Frame(env.currentFrame);
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
	    env.PC++;
	    continue;
	    case "i_enter":
	    env.PC++;
            continue;
            case "i_exitquery":
            {
                return true;
            }
            case "i_exit":
            {
                env.PC = env.currentFrame.PC;
                env.currentFrame = env.currentFrame.parent;
                nextFrame = new Frame(env.currentFrame);
                argP = 0;
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
                nextFrame.PC = env.currentFrame.PC;
                nextFrame.parent = env.currentFrame.parent;
                env.currentFrame = nextFrame;
                nextFrame = new Frame(env.currentFrame);
                argP = 0;
                env.PC = 0; // Start from the beginning of the code in the next frame
                continue;

            }
            case "i_call":
            {
                var functor = Functor.lookup((env.currentFrame.code[env.PC+1] << 8) | (env.currentFrame.code[env.PC+2]));
                nextFrame.functor = functor;
                nextFrame.code = env.getPredicateCode(functor);
                nextFrame.PC = env.PC+3;
                env.currentFrame = nextFrame;
                nextFrame = new Frame(env.currentFrame);
                argP = 0;
                env.PC = 0; // Start from the beginning of the code in the next frame
                continue;
            }
	    case "i_cut":
	    env.PC++;
	    continue;
	    case "i_unify":
	    env.PC++;
	    continue;
            case "b_firstvar":
            {
                var slot = ((env.currentFrame.code[env.PC+1] << 8) | (env.currentFrame.code[env.PC+2]));
                nextFrame.slots[argP++] = env.currentFrame.slots[slot];
                env.PC+=3;
                continue;
            }
            case "b_argvar":
            {
                var slot = ((env.currentFrame.code[env.PC+1] << 8) | (env.currentFrame.code[env.PC+2]));
                env.PC+=3;
                nextFrame.slots[argP++] = env.currentFrame.slots[slot]; // FIXME: Need to trail this?
                continue;
            }
	    case "b_var":
	    env.PC+=3;
	    continue;
	    case "b_pop":
	    env.PC++;
	    continue;
	    case "b_atom":
	    env.PC+=3;
	    continue;
	    case "b_functor":
	    env.PC+=3;
	    continue;
	    case "h_firstvar":
	    env.PC+=3;
	    continue;
            case "h_functor":
	    env.PC+=3;
            continue;
            case "h_pop":
	    env.PC++;
	    continue;
            case "h_atom":
            {
                var atom = AtomTerm.lookup((env.currentFrame.code[env.PC+1] << 8) | (env.currentFrame.code[env.PC+2]));
                var arg = deref(env, env.currentFrame.slots[argP]);
                env.PC+=3;
                if (arg == atom)
                    continue;
                if (arg instanceof VariableTerm)
                {
                    bind(env, arg, atom);
                }
                else
                {
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
                backtrackFrame.code = env.currentFrame.code;
                backtrackFrame.functor = env.currentFrame.functor;
                env.choicepoints.push(backtrackFrame);
                env.PC+=5;
                continue;
            }
            case "trust_me":
	    env.PC+=5;
            continue;
            default:
            {
                console.log("Unknown instruction: " +LOOKUP_Oenv.PCODE[env.currentFrame.code[env.PC]].label);
                throw "illegal instruction";
            }
        }
    }
}

module.exports = {execute: execute};
