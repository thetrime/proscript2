var LOOKUP_OPCODE = require('./opcodes.js').opcodes;
var Functor = require('./functor.js');
var Frame = require('./frame.js');
var Module = require('./module.js');
var util = require('util');
var VariableTerm = require('./variable_term.js');
var AtomTerm = require('./atom_term.js');
var CompoundTerm = require('./compound_term.js');
var Choicepoint = require('./choicepoint.js');

var READ = 0;
var WRITE = 1;

/*
  Overview:

  In general, executing a clause involves two distinct phases. First, we attempt to unify
  arguments in the head with the arguments on the stack. These are the H_ insructions.
  During this phase, the argP[argI] points to the next cell to be matched. If we start matching
  a subterm, for example in the clause
     foo(a, b(c), d):- ....
  then we push the current context to argS, storing argP, argI and the mode (read/write), and
  reinitialize argP to the arguments of the compound we are now trying to match, and argI to 0.
  Initially, when we start executing the head instructions, the value of argP is the array of
  args to the goal.

  To summarize: argP[argI] always points to an argument that was passed IN that we are trying to
  unify with some part of the head of the current clause. The instructions themselves determine
  what we try and match the next cell with

  Once the head is matched, we can start executing the body. This switching of modes is activated
  by the I_ENTER instruction. To understand how to execute the body, suppose the body is made up of
  several sub-goals. For each one, we must push the arguments onto a new frame, then switch
  execution to it. This is accomplished via the B_ instructions. During this phase, argP points
  to the slots array in the next frame, and argI will be set to the nth arg. If we need to push
  a compound, then again we stash the current context and set argP to be the args of the compound.

  Binding is complicated. In a traditional WAM, we have a very clear idea of directionality of
  binding - the address of a variable is strongly related to its cell value. However here, we do not.
  When binding a value X to a variable V (X may or may not /also/ be a variable - more on that in
  a moment), we set two properties of V. First, .value is set to X. This is simple enough. But also
  we set .limit to env.lTop. With a suitable modification to deref(), this means we can unbind
  all variables above the current value of env.lTop, which means that to backtrack we only need to
  decrease lTop to an earlier value.

  There is a complication when X and V are both variables because we have to make explicit which
  way the binding is happening. bind() always binds the var to the value, so if you want to bind
  them in reverse, simply reverse the order of the arguments.

  One consequence of this is that we can no longer do things like comparing variable values to find
  out which way the binding is to happen. See B_ARGVAR

*/


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

function unify(env, a, b)
{
    a = deref(env, a);
    b = deref(env, b);
    if (a.equals(b))
	return true;
    if (a instanceof VariableTerm || b instanceof VariableTerm)
    {
	if (a instanceof VariableTerm && b instanceof VariableTerm)
	{
	    // CHECKME: Is this the right order?
	    if (a.limit > b.limit)
		bind(env, a, b);
	    else
		bind(env, b, a);
	}
	else
	{
	    if (a instanceof VariableTerm)
		bind(env, a, b);
	    else
		bind(env, b, a);
	}
	return true;
    }
    if (a instanceof CompoundTerm && b instanceof CompoundTerm && a.functor.equals(b.functor))
    {
	var lTop = env.lTop;
	for (var i = 0; i < a.args.length; i++)
	{
	    if (!unify(env, a.args[i], b.args[i]))
	    {
		// Undo any bindings we might have done so far
		env.lTop = lTop;
		return false;
	    }
	}
	return true;
    }
    return false;
}

function link(env, value)
{
    if (value === undefined)
        throw "Illegal link";
    var v = new VariableTerm("_l");
    bind(env, v, value);
    return v;
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

function newArgFrame(env)
{
    env.argS.push({p: env.argP,
                   i: env.argI,
		   m: env.mode});
}

function popArgFrame(env)
{
    var argFrame = env.argS.pop();
    env.argP = argFrame.p;
    env.argI = argFrame.i;
    env.mode = argFrame.m;
}

function execute(env)
{
    var currentModule = Module.get("user");
    env.argS = [];
    env.argI = 0;
    env.argP = env.currentFrame.slots;
    env.mode = READ;
    var nextFrame = undefined;
    while(true)
    {
	if (env.currentFrame.code.opcodes[env.PC] === undefined)
        {
            console.log(util.inspect(env.currentFrame));
            console.log("Illegal fetch at " + env.PC);
            throw("Illegal fetch");
        }
	console.log(env.currentFrame.functor + " " + env.PC + ": " + LOOKUP_OPCODE[env.currentFrame.code.opcodes[env.PC]].label);
	switch(LOOKUP_OPCODE[env.currentFrame.code.opcodes[env.PC]].label)
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
                // FIXME: assert(env.argS.length == 0)
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
		env.PC = env.currentFrame.returnPC;
                env.currentFrame = env.currentFrame.parent;
                nextFrame = new Frame(env.currentFrame);
                env.argP = nextFrame.slots;
                env.argI = 0;
		continue;
            }
            case "i_depart":
	    {
		var functor = env.currentFrame.code.constants[((env.currentFrame.code.opcodes[env.PC+1] << 8) | (env.currentFrame.code.opcodes[env.PC+2]))];
		nextFrame.functor = functor;
		nextFrame.code = env.getPredicateCode(functor);
		nextFrame.PC = env.currentFrame.returnPC;
		nextFrame.returnPC = env.currentFrame.returnPC;
		nextFrame.parent = env.currentFrame.parent;
		env.argP = nextFrame.slots;
                env.argI = 0;
                env.currentFrame = nextFrame;
                nextFrame = new Frame(env.currentFrame);
                env.PC = 0; // Start from the beginning of the code in the next frame
                continue;

            }
            case "i_call":
            {
		var functor = env.currentFrame.code.constants[((env.currentFrame.code.opcodes[env.PC+1] << 8) | (env.currentFrame.code.opcodes[env.PC+2]))];
                nextFrame.functor = functor;
                nextFrame.code = env.getPredicateCode(functor);
                nextFrame.returnPC = env.PC+3;
                env.currentFrame = nextFrame;
                // Reset argI to be at the start of the current array. argP SHOULD already be slots for the next frame since we were
                // just filling it in
                // FIXME: assert(env.argS.length === 0)
                // FIXME: assert(env.argP === env.currentFrame.slots)
                env.argI = 0;
                nextFrame = new Frame(env.currentFrame);
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
		// When we get here argI points to the next *free* space in the frame
		// So first we must reduce it
		env.argI--;
		var arg1 = env.argP[env.argI--];
		var arg2 = env.argP[env.argI];
		// The space formerly held by arg2 will now be free again by virtue of env.argI pointing to it
		if (unify(env, arg1, arg2))
		{
		    //console.log("iUnify: Unified " + util.inspect(arg1) + " and " + util.inspect(arg2));
		    env.PC++;
		    continue;
		}
		console.log("iUnify: Failed to unify " + util.inspect(arg1) + " and " + util.inspect(arg2));
                if (backtrack(env))
                    continue;
                return false;

            }
            case "b_firstvar":
            {
		var slot = ((env.currentFrame.code.opcodes[env.PC+1] << 8) | (env.currentFrame.code.opcodes[env.PC+2]));
                env.argP[env.argI++] = env.currentFrame.slots[slot];
                env.PC+=3;
                continue;
            }
            case "b_argvar":
            {
		var slot = ((env.currentFrame.code.opcodes[env.PC+1] << 8) | (env.currentFrame.code.opcodes[env.PC+2]));
		var arg = env.currentFrame.slots[slot];
		arg = deref(env, arg);
		//console.log("     " + util.inspect(arg));
		if (arg instanceof VariableTerm)
                {
                    // FIXME: This MAY require trailing
		    env.argP[env.argI] = new VariableTerm("_a");
		    bind(env, env.argP[env.argI], arg);
		    //console.log("argP now refers to " + util.inspect(env.argP[env.argI]));
                }
                else
                {
                    env.argP[env.argI] = arg;
                }
                env.argI++;
		env.PC+=3;
                continue;
            }
            case "b_var":
            {
		var slot = ((env.currentFrame.code.opcodes[env.PC+1] << 8) | (env.currentFrame.code.opcodes[env.PC+2]));
		env.argP[env.argI] = link(env, env.currentFrame.slots[slot]);
                env.argI++;
                env.PC+=3;
                continue;
            }
            case "b_pop":
            {
		popArgFrame(env);
                env.PC++;
                continue;
            }
            case "b_atom":
	    {
		var index = ((env.currentFrame.code.opcodes[env.PC+1] << 8) | (env.currentFrame.code.opcodes[env.PC+2]));
		env.argP[env.argI++] = env.currentFrame.code.constants[index];
		env.PC+=3;
                continue;
            }
            case "b_functor":
            {
		var index = ((env.currentFrame.code.opcodes[env.PC+1] << 8) | (env.currentFrame.code.opcodes[env.PC+2]));
		var functor = env.currentFrame.code.constants[index];
		var new_term_args = new Array(functor.arity);
		env.argP[env.argI++] = new CompoundTerm(functor, new_term_args);
		newArgFrame(env);
		env.argP = new_term_args;
		env.argI = 0;
		env.PC+=3;
		continue;
            }
            case "h_firstvar":
	    {
		// varFrame(FR, *PC++) in SWI-Prolog is the same as
		// env.currentFrame.slot[(env.currentFrame.code.opcodes[env.PC+1] << 8) | (env.currentFrame.code.opcodes[env.PC+2])] in PS2
                // basically varFrameP(FR, n) is (((Word)f) + n), which is to say it is a pointer to the nth word in the frame
                // In PS2 parlance, these are 'slots'
		var slot = ((env.currentFrame.code.opcodes[env.PC+1] << 8) | (env.currentFrame.code.opcodes[env.PC+2]));
                if (env.mode == WRITE) // write
		{
		    // If in write mode, we must create a variable in the current frame's slot, then bind it to the next arg to be matched
                    // This happens if we are trying to match a functor to a variable. To do that, we create a new term with the right functor
                    // but all the args as variables, pushed the current state to argS, then continue matching with argP pointing to the args
                    // of the functor we just created. In this particular instance, we want to match one of the args (specifically, argI) to
                    // a variable occurring in the head, for example the X in foo(a(X)):- ...
                    // the compiler has allocated an appropriate slot in the frame above the arity of the goal we are executing
                    // we just need to create a variable in that slot, then bind it to the variable in the current frame
		    env.currentFrame.slots[slot] = new VariableTerm("_f");
		    bind(env, env.argP[env.argI], env.currentFrame.slots[slot]);
		}
                else
                {
                    // in read mode, we are trying to match an argument to a variable occurring in the head. If the thing we are trying to
                    // match is not a variable, put a new variable in the slot and bind it to the thing we actually want
                    // otherwise, we can just copy the variable directly into the slot
                    if (env.argP[env.argI] instanceof VariableTerm)
                    {
                        env.currentFrame.slots[slot] = env.argP[env.argI]
                    }
                    else
		    {
			env.currentFrame.slots[slot] = new VariableTerm("_X");
			bind(env, env.currentFrame.slots[slot], env.argP[env.argI]);
                    }
                }
                // H_FIRSTVAR always succeeds - so move on to match the next cell, and increment PC for the next instructions
                env.argI++;
                env.PC+=3;
                continue;
            }
            case "h_functor":
            {
		var functor = env.currentFrame.code.constants[((env.currentFrame.code.opcodes[env.PC+1] << 8) | (env.currentFrame.code.opcodes[env.PC+2]))];
		var arg = deref(env, env.argP[env.argI++]);
                env.PC+=3;
                // Try to match a functor
                if (arg instanceof CompoundTerm)
                {
		    if (arg.functor.equals(functor))
		    {
			newArgFrame(env);
                        env.argP = arg.args;
                        env.argI = 0;
                        continue;
                    }
                }
                else if (arg instanceof VariableTerm)
		{
		    newArgFrame(env);
                    var args = new Array(functor.arity);
                    for (var i = 0; i < args.length; i++)
			args[i] = new VariableTerm("_ff");
		    bind(env, arg, new CompoundTerm(functor, args));
		    env.argP = args;
		    env.argI = 0;
		    env.mode = WRITE;
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
                popArgFrame(env);
                env.PC++;
                continue;
            }
            case "h_atom":
            {
		var atom = env.currentFrame.code.constants[((env.currentFrame.code.opcodes[env.PC+1] << 8) | (env.currentFrame.code.opcodes[env.PC+2]))];
                var arg = deref(env, env.argP[env.argI++]);
                env.PC+=3;
		if (arg.equals(atom))
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
		var address = (env.currentFrame.code.opcodes[env.PC+1] << 24) | (env.currentFrame.code.opcodes[env.PC+2] << 16) | (env.currentFrame.code.opcodes[env.PC+3] << 8) | (env.currentFrame.code.opcodes[env.PC+4] << 0) + env.PC;
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
		console.log("Unknown instruction: " +LOOKUP_Oenv.PCODE[env.currentFrame.code.opcodes[env.PC]].label);
                throw "illegal instruction";
            }
        }
    }
}

module.exports = {execute: execute};
