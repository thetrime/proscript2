"use strict";

var Constants = require('./constants.js');
var LOOKUP_OPCODE = require('./opcodes.js').opcodes;
var Functor = require('./functor.js');
var Compiler = require('./compiler.js');
var Frame = require('./frame.js');
var Module = require('./module.js');
var util = require('util');
var VariableTerm = require('./variable_term.js');
var AtomTerm = require('./atom_term.js');
var CompoundTerm = require('./compound_term.js');
var Choicepoint = require('./choicepoint.js');
var Instructions = require('./opcodes.js').opcode_map;
var READ = 0;
var WRITE = 1;

/*
  Overview:

  In general, executing a clause involves two distinct phases. First, we attempt to unify
  arguments in the head with the arguments on the stack. These are the H_ insructions.
  During this phase, the argP[argI] points to the next cell to be unified. Note that argP might
  not be a pointer to the previously constructed frame; for example, if we have just tried to
  unify with a compound term, then after matching the functor argP will then point to the
  arguments of the term, and argI will be set to 0 so that argP[argI] is not the first arg of the
  term we are trying to match. argS is a stack of previous values of argP so that after we are
  done matching the term we can execute an instruction (h_pop) to reset argP back to where it
  was before we detoured.

  To summarize: argP[argI] always points to an argument that was passed IN that we are trying to
  unify with some part of the head of the current clause. The instructions themselves determine
  what we try and match the next cell with

  Once the head is matched, we can start executing the body. This switching of modes is activated
  by the i_enter instruction. To understand how to execute the body, suppose the body is made up of
  several sub-goals. For each one, we must push the arguments onto a new frame, then switch
  execution to it. This is accomplished via the B_ instructions. During this phase, argP points
  to the slots array in the next frame, and argI will be set to the nth arg. If we need to push
  a compound, then again we stash the current context and set argP to be the args of the compound.

  Binding is complicated. In a traditional WAM, we have a very clear idea of directionality of
  binding - the address in memory of a variable is strongly related to its cell value. However here,
  this is not the case.  When binding a value X to a variable V (X may or may not /also/ be a variable
  - more on that in a moment), we set two properties of V. First, .value is set to X. This is simple
  enough. But also we set .limit to env.TR, which gives each variable a 'location'.

  There is a complication when X and V are both variables because we have to make explicit which
  way the binding is happening. bind() always binds the var to the value, so if you want to bind
  them in reverse, simply reverse the order of the arguments.

  One consequence of this is that we can no longer do things like comparing variable values to find
  out which way the binding is to happen. See B_ARGVAR

*/

var nextvar = 0;

function unwind_trail(env)
{
    // Unwind the trail back to env.TR
    for (var i = env.TR; i < env.trail.length; i++)
        env.trail[i].value = null;
    env.trail = env.trail.slice(0, env.TR);
}

function link(env, value)
{
    if (value === undefined)
        throw "Illegal link";
    var v = new VariableTerm("_l");
    env.bind(v, value);
    return v;
}

function copyTerm(t)
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

function restore_state(env, choicepoint)
{
    env.currentFrame = choicepoint.frame;
    env.PC = choicepoint.retryPC;
    env.TR = choicepoint.retryTR;
    env.argP = choicepoint.argP;
    env.argI = choicepoint.argI;
    env.argS = choicepoint.argS;
    env.nextFrame = choicepoint.nextFrame;
}

function unwind_stack(env, choicepoint)
{
    restore_state(env, choicepoint);
    unwind_trail(env);
}

function backtrack_to(env, choicepoint_index)
{
    // Backtracks to the given choicepoint, undoing everything else in between
    for (var i = env.choicepoints.length-1; i >= choicepoint_index-1; i--)
    {
        console.log("Unwinding a choicepoint");
        unwind_stack(env, env.choicepoints[i]);
    }
    env.choicepoints = env.choicepoints.slice(0, choicepoint_index);
}

function backtrack(env)
{
    if (env.choicepoints.length == 0)
        return false;
    var choicepoint = env.choicepoints.pop();
    unwind_stack(env, choicepoint);
    if (choicepoint.retryPC == -1)
    {
        // This is a fake choicepoint set up for something like exception handling. We have to keep going
        return backtrack(env);
    }
    return true;
}

function cut_to(env, point)
{
    while (env.choicepoints.length > point)
    {
        var c = env.choicepoints.pop();
        if (c.cleanup != undefined)
        {
            // Run the cleanup if the catcher unifies. Basically, since this is SO far outside the normal execution flow,
            // just run it as if it were a new top-level query
            if (env.unify(c.cleanup.catcher, Constants.cutAtom))
            {
                // What do we do about exceptions, failures etc? For now, just ignore them
                var savedState = new Choicepoint(env, env.PC);
                try
                {
                    var compiledCode = Compiler.compileQuery(c.cleanup.goal);

                    // make a frame with 0 args (since a query has no head)
                    env.currentFrame = new Frame(env);
                    env.currentFrame.functor = new Functor(new AtomTerm("$cleanup"), 0);
                    env.currentFrame.code = {opcodes: [Instructions.iExitQuery.opcode],
                                             constants: []};
                    env.nextFrame.parent = env.currentFrame;
                    env.nextFrame.functor = new Functor(new AtomTerm("call"), 1);
                    env.nextFrame.code = {opcodes: compiledCode.bytecode,
                                          constants: compiledCode.constants};
                    env.nextFrame.returnPC = 0; // Return to exit_query

                    env.currentFrame = env.nextFrame;
                    env.argP = env.currentFrame.slots;
                    env.argI = 0;
                    for (var i = 0; i < compiledCode.variables.length; i++)
                        env.currentFrame.slots[i] = compiledCode.variables[i];
                    env.nextFrame = new Frame(env);
                    env.PC = 0; // Start from the beginning of the code in the next frame
                    console.log("Executing handler " + c.cleanup.goal);
                    console.log("With args: " + util.inspect(env.argP, {showHidden: false, depth: null}));
                    this.execute(env);
                }
                catch(ignored)
                {
                }
                finally
                {
                    restore_state(env, savedState);
                }
            }
        }
    }
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

var debugger_steps = 0;
var next_opcode = undefined;
var exception = undefined;
function execute(env)
{
    var current_opcode;
    next_instruction:
    while (!env.halted)
    {
        if (next_opcode === undefined && env.currentFrame.code.opcodes[env.PC] === undefined)
        {
            console.log(util.inspect(env.currentFrame));
            console.log("Illegal fetch at " + env.PC);
            throw new Error("Illegal fetch");
        }
        current_opcode = (next_opcode || (next_opcode = LOOKUP_OPCODE[env.currentFrame.code.opcodes[env.PC]].label));
        next_opcode = undefined;
        debugger_steps ++;
        //if (debugger_steps == 50) throw(0);
        console.log("@ " + env.currentModule.name + ":" + env.currentFrame.functor + " " + env.PC + ": " + current_opcode);
        switch(current_opcode)
	{
            case "i_fail":
            {
                if (backtrack(env))
                    continue;
                return false;
            }
            case "i_true":
            {
                env.PC++;
                continue;
            }
            case "i_enter":
            {
                env.nextFrame = new Frame(env);
                env.argI = 0;
                env.argP = env.nextFrame.slots;
                // FIXME: assert(env.argS.length == 0)
                env.PC++;
                continue;
            }
            case "i_exitquery":
            {
                return true;
            }
            case "i_exitcatch":
            {
                var slot = ((env.currentFrame.code.opcodes[env.PC+1] << 8) | (env.currentFrame.code.opcodes[env.PC+2]));
                if (env.currentFrame.reserved_slots[slot] == env.choicepoints.length)
                {
                    // Goal has exited deterministically, we do not need our backtrack point anymore since there is no way we could
                    // backtrack into Goal and generate an exception.
                    // Note that it is possible, if we executed the handler, that the fake choicepoint has already been deleted
                    env.choicepoints.pop();
                }
                next_opcode = "i_exit";
                continue next_instruction;
            }
            case "i_foreign":
            {
                env.currentFrame.reserved_slots[0] = undefined;
                // FALL-THROUGH
            }
            case "i_foreignretry":
            {
                env.foreign = env.currentFrame.reserved_slots[0];
                var args = env.currentFrame.slots.slice(0);
                for (var i = 0; i < args.length; i++)
                    args[i] = args[i].dereference();
                // Set the foreign info to undefined
                var rc = false;
                try
                {
                    rc = env.currentFrame.code.constants[0].apply(env, args);
                }
                catch (e)
                {
                    exception = e;
                    console.log("Foreign result: " + exception);
                    next_opcode = "b_throw_foreign";
                    continue next_instruction;
                }
                console.log("Foreign result: " + rc);
                if (rc == 0)
                {
                    // CHECKME: Does this undo any partial bindings that happen in a failed foreign frame?
                    if (backtrack(env))
                        continue;
                    return false;
                }
                next_opcode = "i_exit";
                continue next_instruction;
            }
            case "i_exit":
	    {
		env.PC = env.currentFrame.returnPC;
                env.currentFrame = env.currentFrame.parent;
                env.nextFrame = new Frame(env);
                env.argP = env.nextFrame.slots;
                env.argI = 0;
                continue;
            }
	    case "i_exitfact":
	    {
		env.PC = env.currentFrame.returnPC;
                env.currentFrame = env.currentFrame.parent;
                env.nextFrame = new Frame(env);
                env.argP = env.nextFrame.slots;
                env.argI = 0;
		continue;
            }
            case "i_depart":
	    {
		var functor = env.currentFrame.code.constants[((env.currentFrame.code.opcodes[env.PC+1] << 8) | (env.currentFrame.code.opcodes[env.PC+2]))];
		env.nextFrame.functor = functor;
		env.nextFrame.code = env.getPredicateCode(functor);
		env.nextFrame.PC = env.currentFrame.returnPC;
		env.nextFrame.returnPC = env.currentFrame.returnPC;
		env.nextFrame.parent = env.currentFrame.parent;
		env.argP = env.nextFrame.slots;
                env.argI = 0;
                env.currentFrame = env.nextFrame;
                env.nextFrame = new Frame(env);
                env.PC = 0; // Start from the beginning of the code in the next frame
                continue;
            }
            case "b_cleanup_choicepoint":
            {
                // This just leaves a fake choicepoint (one you cannot backtrack onto) with a .cleanup value set to the cleanup handler
                var c = new Choicepoint(env, -1);
                console.log("Goal: " + env.argP[env.argI-1]);
                console.log("Catcher: " + env.argP[env.argI-2]);
                c.cleanup = {goal: env.argP[env.argI-1],
                             catcher: env.argP[env.argI-2]};
                env.argP-=2;
                env.choicepoints.push(c);
                env.PC++;
                continue;
            }

            case "b_throw":
            {
                // This is the hardest part of the exception handling process.
                // We have to walk the stack, looking for frames that correspond to calls to catch/3, find the unifier in the frame, and try to unify it with argP[argI-1]
                // If unsuccessful (or partially successful!) we must unwind and look further. If nothing is found and we get to the top, throw a real Javascript exception
                // Finally, care must be taken to unwind any choicepoints we find along the way as we are go so that if the exception is caught we cannot later backtrack
                // into a frame that should've been destroyed by the bubbling exception

                // First, make a copy of the ball, since we are about to start unwinding things and we dont want to undo its binding
                exception = copyTerm(env.argP[env.argI-1]);
                // Fall-through
            }
            case "b_throw_foreign":
            {
                var frame = env.currentFrame;
                console.log(util.inspect(env.choicepoints));
                while (frame != undefined)
                {
                    if (frame.functor.equals(Constants.catchFunctor))
                    {
                        // Unwind to the frames choicepoint. Note that this will be set to the fake choicepoint we created in i_catch
                        console.log("Frame " + frame.functor + " has choicepoint of " + frame.choicepoint);
                        console.log("Env has " + env.choicepoints.length + " choicepoints");
                        backtrack_to(env, frame.choicepoint);
                        // Try to unify (a copy of) the exception with the unifier
                        if (env.unify(copyTerm(exception), frame.slots[1]))
                        {
                            // Success! Exception is successfully handled. Now we just have to do i_usercall after adjusting the registers to point to the handler
                            // Things get a bit weird here because we are breaking the usual logic flow by ripping the VM out of whatever state it was in and starting
                            // it off in a totally different place. We have to reset argP, argI and PC then pretend the next instruction was i_usercall
                            env.argP = env.currentFrame.slots;
                            env.argI = 3;
                            env.PC = 6; // After we have executed the handler we need to go to the i_exitcatch which chould be at PC=6+1
                            // Javascript doesnt have a goto statement, so this is a bit tricky
                            next_opcode = "i_usercall";
                            console.log("goto i_usercall");
                            continue next_instruction;
                        }
                    }
                    frame = frame.parent;
                }
                throw new Error("unhandled exception:" + exception);
            }
            case "i_switch_module":
            {
                // FIXME: We must make sure that when processing b_throw that we pop modules as needed
                // FIXME: There could also be implications for the cleanup in setup-call-cleanup?
                var module = env.currentFrame.code.constants[((env.currentFrame.code.opcodes[env.PC+1] << 8) | (env.currentFrame.code.opcodes[env.PC+2]))];
                env.moduleStack.push(env.currentModule);
                env.currentModule = Module.get(module.value);
                env.PC+=3;
                continue;
            }
            case "i_exitmodule":
            {
                env.currentModule = env.moduleStack.pop();
                env.PC++;
                continue;
            }
            case "i_call":
            {
                var functor = env.currentFrame.code.constants[((env.currentFrame.code.opcodes[env.PC+1] << 8) | (env.currentFrame.code.opcodes[env.PC+2]))];
                env.nextFrame.functor = functor;
                env.nextFrame.code = env.getPredicateCode(functor);
                env.nextFrame.returnPC = env.PC+3;
                env.currentFrame = env.nextFrame;
                // Reset argI to be at the start of the current array. argP SHOULD already be slots for the next frame since we were
                // just filling it in
                // FIXME: assert(env.argS.length === 0)
                // FIXME: assert(env.argP === env.currentFrame.slots)
                env.argI = 0;
                env.nextFrame = new Frame(env);
                env.PC = 0; // Start from the beginning of the code in the next frame
                continue;
            }
            case "i_catch":
            {
                var slot = ((env.currentFrame.code.opcodes[env.PC+1] << 8) | (env.currentFrame.code.opcodes[env.PC+2]));
                // i_catch must do a few things:
                //    * Create a backtrack point at the start of the frame so we can undo anything in the guarded goal if something goes wrong
                //    * ensure argP[argI-1] points to the goal
                //    * Jump to i_usercall to call the goal
                env.choicepoints.push(new Choicepoint(env, -1));
                // Since catch/3 can never itself contain a cut (the bytecode is fixed at i_catch, i_exitcatch), we can afford to tweak the frame a bit
                // by moving the frames cut choicepoint to the fake choicepoint we have just created, we can simplify b_throw
                env.currentFrame.choicepoint = env.choicepoints.length;
                env.currentFrame.reserved_slots[slot] = env.choicepoints.length;
                console.log(env.currentFrame.slots);
                // Currently argP points to the first slot in the next frame that we have started building since we've done i_enter already
                // we need to set it back to the slots of the /current/ frame so that i_usercall finds the goal
                env.argP = env.currentFrame.slots;
                env.argI = 1;
                env.PC+=2; // i_usercall will add the third byte in a moment
                // Fall through to i_usercall
            }
            case "i_usercall":
            {
                // argP[argI-1] is a goal that we want to execute. First, we must compile it
                env.argI--;
                console.log("argP:" + env.argP);
                var goal = env.argP[env.argI].dereference();
                console.log("Goal: " + goal);
                var compiledCode = Compiler.compileQuery(goal);
                env.nextFrame.functor = new Functor(new AtomTerm("call"), 1);
                env.nextFrame.code = {opcodes: compiledCode.bytecode,
                                      constants: compiledCode.constants};
                env.nextFrame.returnPC = env.PC+1;
                env.currentFrame = env.nextFrame;
                env.argP = env.currentFrame.slots;
                env.argI = 0;
                // Now the arguments need to be filled in. This takes a bit of thought.
                // What we have REALLY done is taken Goal and created a new, local predicate '$query'/N like this:
                //     '$query'(A, B, C, ....Z):-
                //           Goal.
                // Where the variables of Goal are A, B, C, ...Z. This implies we must now push the arguments of $query/N
                // onto the stack, and NOT the arguments of Goal, since we are about to 'call' $query.
                console.log("Vars: " + compiledCode.variables);
                for (var i = 0; i < compiledCode.variables.length; i++)
                    env.currentFrame.slots[i] = compiledCode.variables[i];
                env.nextFrame = new Frame(env);
                env.PC = 0; // Start from the beginning of the code in the next frame
                console.log("Executing " + goal);
                console.log("With args: " + util.inspect(env.argP, {showHidden: false, depth: null}));
                continue;
            }
            case "i_cut":
            {
                // The task of i_cut is to pop and discard all choicepoints newer than env.currentFrame.choicepoint
                console.log("Cutting choicepoints to " + env.currentFrame.choicepoint);
                // If we want to support cleanup, we cannot just do this:
                //env.choicepoints = env.choicepoints.slice(0, env.currentFrame.choicepoint);
                cut_to(env, env.currentFrame.choicepoint);
                env.currentFrame.choicepoint = env.choicepoints.length;
                env.PC++;
                continue;
            }
            case "c_cut":
            {
                // The task of c_cut is to pop and discard all choicepoints newer than the value in the given slot
                var slot = ((env.currentFrame.code.opcodes[env.PC+1] << 8) | (env.currentFrame.code.opcodes[env.PC+2]));
                cut_to(env, env.currentFrame.reserved_slots[slot]);
                env.PC+=3;
                continue;
            }
            case "c_ifthen":
            {
                var slot = ((env.currentFrame.code.opcodes[env.PC+1] << 8) | (env.currentFrame.code.opcodes[env.PC+2]));
                env.currentFrame.reserved_slots[slot] = env.choicepoints.length;
                env.PC+=3;
                continue;
            }
            case "c_ifthenelse":
            {
                var slot = ((env.currentFrame.code.opcodes[env.PC+1] << 8) | (env.currentFrame.code.opcodes[env.PC+2]));
                var address = (env.currentFrame.code.opcodes[env.PC+1] << 24) | (env.currentFrame.code.opcodes[env.PC+2] << 16) | (env.currentFrame.code.opcodes[env.PC+3] << 8) | (env.currentFrame.code.opcodes[env.PC+4] << 0) + env.PC;
                env.currentFrame.reserved_slots[slot] = env.choicepoints.length;
                env.choicepoints.push(new Choicepoint(env, address));
                env.PC+=7;
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
                if (env.unify(arg1, arg2))
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
                console.log("firstvar: setting slot " + slot + " to a new variable");
                env.currentFrame.slots[slot] = new VariableTerm("_L" + nextvar++);
                env.argP[env.argI++] = link(env, env.currentFrame.slots[slot]);
                env.PC+=3;
                continue;
            }
            case "b_argvar":
            {
                var slot = ((env.currentFrame.code.opcodes[env.PC+1] << 8) | (env.currentFrame.code.opcodes[env.PC+2]));
                console.log("argvar: getting variable from slot " + slot);
		var arg = env.currentFrame.slots[slot];
                arg = arg.dereference();
                console.log("Value of variable is: " + util.inspect(arg, {showHidden: false, depth: null}));
                if (arg instanceof VariableTerm)
                {
                    // FIXME: This MAY require trailing
                    env.argP[env.argI] = new VariableTerm("_A" + nextvar++);
		    env.bind(env.argP[env.argI], arg);
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
                    env.currentFrame.slots[slot] = new VariableTerm("_L" + nextvar++);
                    env.bind(env.argP[env.argI], env.currentFrame.slots[slot]);
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
                        env.currentFrame.slots[slot] = new VariableTerm("_X" + nextvar++);
			env.bind(env.currentFrame.slots[slot], env.argP[env.argI]);
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
                var arg = env.argP[env.argI++].dereference();
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
                        args[i] = new VariableTerm("_f" + nextvar++);
		    env.bind(arg, new CompoundTerm(functor, args));
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
                var arg = env.argP[env.argI++].dereference();
                env.PC+=3;
		if (arg.equals(atom))
                    continue;
                if (arg instanceof VariableTerm)
                {
                    env.bind(arg, atom);
                }
                else
                {
                    console.log("argP: " + util.inspect(env.argP, {showHidden: false, depth:null}));
                    console.log("argI: " + env.argI);
                    console.log("Failed to unify " + util.inspect(arg) + " with the atom " + util.inspect(atom));
                    if (backtrack(env))
                        continue;
                    return false;
                }
                continue;
            }
            case "h_void":
            {
                env.argI++;
	        env.PC++;
	        continue;
            }
	    case "h_var":
            {
                var slot = ((env.currentFrame.code.opcodes[env.PC+1] << 8) | (env.currentFrame.code.opcodes[env.PC+2]));
                var arg = env.argP[env.argI++].dereference();
                if (!env.unify(arg, env.currentFrame.slots[slot]))
                {
                    if (backtrack(env))
                        continue;
                    return false;
                }
                env.PC+=3;
		continue;
	    }
	    case "c_jump":
	    {
                env.PC = (env.currentFrame.code.opcodes[env.PC+1] << 24) | (env.currentFrame.code.opcodes[env.PC+2] << 16) | (env.currentFrame.code.opcodes[env.PC+3] << 8) | (env.currentFrame.code.opcodes[env.PC+4] << 0) + env.PC;
		continue;
	    }
	    case "c_or":
            case "try_me_else":
            case "retry_me_else":
            {
                var address = (env.currentFrame.code.opcodes[env.PC+1] << 24) | (env.currentFrame.code.opcodes[env.PC+2] << 16) | (env.currentFrame.code.opcodes[env.PC+3] << 8) | (env.currentFrame.code.opcodes[env.PC+4] << 0) + env.PC;
                env.choicepoints.push(new Choicepoint(env, address));
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
		throw new Error("illegal instruction: " + LOOKUP_OPCODE[env.currentFrame.code.opcodes[env.PC]].label);
            }
        }
    }
}

module.exports = {execute: execute,
                  backtrack: backtrack,

                  copyTerm: copyTerm};
