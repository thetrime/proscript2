"use asm";
exports=module.exports;

var Constants = require('./constants.js');
var Opcodes = require('./opcodes.js').opcodes;
var LOOKUP_OPCODE = require('./opcodes.js').opcode_map;
var Functor = require('./functor.js');
var Compiler = require('./compiler.js');
var Frame = require('./frame.js');
var util = require('util');
var AtomTerm = require('./atom_term.js');
var CompoundTerm = require('./compound_term.js');
var Choicepoint = require('./choicepoint.js');
var ClauseChoicepoint = require('./clause_choicepoint.js');
var Errors = require('./errors.js');
var Clause = require('./clause.js');
var CTable = require('./ctable.js');
var Instructions = require('./opcodes.js').opcode_map;

/*
  Overview:

  In general, executing a clause involves two distinct phases. In phase 1, we attempt to unify
  arguments in the head with the arguments on the stack. These are the H_ insructions.
  During this phase, argP[argI] points to the next cell to be unified. Note that argP might
  not be a pointer to the previously constructed frame; for example, if we have just tried to
  unify with a compound term, then after matching the functor argP will then point to the
  arguments of the term, and argI will be set to 0 so that argP[argI] is now the first arg of the
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
  to the next place to build the argument for the goal. Initially, it points to the next frame's .slots
  array, but after executing b_functor, it points to the first arg of a constructed term instead. b_pop
  restores argP to the parent, as with h_pop.

  Binding is complicated. Currently, *everything* is trailed, which is not incorrect, but very
  inefficient. Some work needs to be done to determine cases where trailing is not needed. These are
  any cases where the variable points /backward/ in the call stack. (For example, if a frame contains
  a local variable, and that local variable is bound to a value in the parent frame, then the binding
  can be 'undone' just by forgetting about the variable.

*/


function unwind_trail(env, from)
{
    // Unwind the trail back to env.TR
    for (var i = env.TR; i < from; i++)
        HEAP[env.trail[i]] = env.trail[i];
}

function link(env, value)
{
    if (value === undefined)
        throw "Illegal link";
    var v = MAKEVAR();
    env.bind(v, value);
    return v;
}

function set_exception(env, term)
{
    if (term.stack != undefined)
    {
        console.log("Trace: " + term.stack);
        exception = CompoundTerm.create(Constants.systemErrorFunctor, [term.toString()]);
    }
    else
    {
        exception = env.copyTerm(term);
    }

}

function backtrack_to(env, choicepoint_index)
{
    // Undoes to the given choicepoint, undoing everything else in between
    for (var i = env.choicepoints.length-1; i >= choicepoint_index-1; i--)
    {
        var oldTR = env.TR;
        env.choicepoints[i].apply(env);
        unwind_trail(env, oldTR);
    }
    env.choicepoints = env.choicepoints.slice(0, choicepoint_index);
}

function backtrack(env)
{
    //    env.debug_backtracks++;
    //console.log("... backtracking");
    var oldTR = env.TR;
    while (true)
    {
        if (env.choicepoints.length == 0)
            return false;
        var choicepoint = env.choicepoints.pop();
        if (!choicepoint.apply(env))
        {
            // This is a fake choicepoint set up for something like exception handling. We have to keep going
            continue;
        }
        unwind_trail(env, oldTR);
        return true;
    }
}

function resume_cut(env, savedState)
{
    return function()
    {
        // This just retries the same machine instruction that lead us to cut_to in the first place
        // However, since we first tried it at least one choicepoint would have been popped off, and
        // eventually the cut_to will just exit and execution can continue in the parent VM
        env.currentModule = savedState.currentModule;
        env.currentFrame = savedState.currentFrame;
        env.nextFrame = savedState.nextFrame;
        env.choicepoints = savedState.choicepoints;
        env.argS = [];
        env.argP = env.nextFrame.slots;
        env.argI = 0;
        env.PC = savedState.PC;
        env.mode = savedState.mode;
        env.streams = savedState.stream;
        env.yieldInfo = savedState.yieldInfo;
        redo_execute(env);
    }
}

function cut_to(env, point)
{
    while (env.choicepoints.length > point)
    {
        var c = env.choicepoints.pop();
        if (c.cleanup != undefined)
        {
            if (c.cleanup.foreign != undefined)
            {
                // This is a foreign cleanup frame. Just execute the code
                c.cleanup.foreign.call(env);
            }
            else
            {
                // Run the cleanup if the catcher unifies. Basically, since this is SO far outside the normal execution flow,
                // just run it as if it were a new top-level query
                if (env.unify(c.cleanup.catcher, Constants.cutAtom))
                {
                    // What do we do about exceptions, failures etc? For now, just ignore them
                    // Note that this is different to saveState. In particular, we preserve the trail
                    var savedState = {currentModule: env.currentModule,
                                      currentFrame: env.currentFrame,
                                      nextFrame: env.nextFrame,
                                      choicepoints: env.choicepoints,
                                      PC: env.PC,
                                      stream: env.streams,
                                      yieldInfo: env.yieldInfo};
                    try
                    {
                        var compiledCode = Compiler.compileQuery(c.cleanup.goal);

                        // Start afresh with no choicepoints
                        env.choicepoints = [];

                        // make a frame with 0 args (since a query has no head)
                        env.currentFrame = new Frame(env);
                        env.currentFrame.functor = Functor.get(AtomTerm.get("$cleanup"), 0);
                        env.currentFrame.clause = new Clause([Instructions.iExitQuery.opcode], [], ["i_exit_query"]);
                        env.nextFrame.parent = env.currentFrame;
                        env.nextFrame.functor = Functor.get(AtomTerm.get("<meta-call>"), 1);
                        env.nextFrame.clause = compiledCode.clause;
                        env.nextFrame.returnPC = 0; // Return to exit_query

                        env.currentFrame = env.nextFrame;
                        env.argP = env.currentFrame.slots;
                        env.argI = 0;
                        for (var i = 0; i < compiledCode.variables.length; i++)
                            env.currentFrame.slots[i] = compiledCode.variables[i];
                        env.nextFrame = new Frame(env);
                        env.PC = 0; // Start from the beginning of the code in the next frame
                        //console.log("Executing handler " + c.cleanup.goal);
                        //console.log("With args: " + util.inspect(env.argP, {showHidden: false, depth: null}));
                        // Yikes. This is a bit complicated :(
                        var resumeFn = resume_cut(env, savedState);
                        execute(env, resumeFn, resumeFn, resumeFn);
                        // We cannot just keep going at this point since the execute() might be exiting because of a yield.
                        // At this point we have basically forked and should immediately exit. Once the cleanup goal has run it
                        // will re-enter via a call from resumeFn to redo_execute()
                        // Since the kernel will resume from the cut again, we have more opportunities here to cut further choicepoints if need be

                        return false;
                    }
                    catch(ignored)
                    {
                        console.log(ignored);
                    }
                }
            }
        }
    }
    // If we get here then we did NOT run any handler and we can return and continue executing the (old) machine
    return true;
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

function print_opcode(env, opcode)
{
    var opcode_info = Opcodes[opcode];
    var s = opcode_info.label;
    var ptr = env.PC+1;
    for (var j = 0; j < opcode_info.args.length; j++)
    {
        if (opcode_info.args[j] == "slot")
        {
            s += "(" + ((env.currentFrame.clause.opcodes[ptr++] << 8) | (env.currentFrame.clause.opcodes[ptr++])) + ")";
        }
        else if (opcode_info.args[j] == "address")
        {
            s += " " + ((env.currentFrame.clause.opcodes[ptr++] << 24) | (env.currentFrame.clause.opcodes[ptr++] << 16) | (env.currentFrame.clause.opcodes[ptr++] << 8) | (env.currentFrame.clause.opcodes[ptr++] << 0) + env.PC)
        }
        else if (opcode_info.args[j] == "functor")
        {
            s += "(" + CTable.get(env.currentFrame.clause.constants[((env.currentFrame.clause.opcodes[ptr++] << 8) | (env.currentFrame.clause.opcodes[ptr++]))]).toString() + ")";
        }
        else if (opcode_info.args[j] == "atom")
        {
            s += "(" + PORTRAY(env.currentFrame.clause.constants[((env.currentFrame.clause.opcodes[ptr++] << 8) | (env.currentFrame.clause.opcodes[ptr++]))]) + ")";
        }
    }
    return s;
}

function pad(pad, str)
{
    return (str + pad).substring(0, pad.length);
}

function print_instruction(env, current_opcode)
{
    var module = env.currentFrame.contextModule;
    if (module === undefined)
        module = {name:"<undefined>"};
    //console.log("Depth: " + env.currentFrame.depth);
    console.log(pad("                                                            ", ("@ " + module.name + ":" + CTable.get(env.currentFrame.functor).toString() + " " + env.PC + ": ")) + print_opcode(env, current_opcode));
}

var next_opcode = undefined;
var exception = undefined;

function execute(env, successHandler, onFailure, onError)
{
    env.yieldInfo = {initial_choicepoint_depth: env.choicepoints.length,
                     onSuccess:successHandler,
                     onFailure:onFailure,
                     onError:onError};
    //var d0 = new Date().getTime();
    redo_execute(env);
    //console.log("execute() took " + (new Date().getTime() - d0) + "ms");
}

function execute_foreign(env, args)
{
    try
    {
      return env.currentFrame.clause.constants[0].apply(env, args);
    }
    catch(e)
    {
        console.log("Error in " + CTable.get(env.currentFrame.functor).toString());
        set_exception(env, e);
        return "exception";
    }
}

function get_code(env, functor)
{
    try
    {
        env.nextFrame.clause = env.getPredicateCode(functor, env.currentFrame.contextModule);
        env.nextFrame.contextModule = env.nextFrame.clause.module;
        return true;
    }
    catch(e)
    {
        set_exception(env, e);
        return false;
    }
}

function usercall(env, goal)
{
    var compiledCode;
    try
    {
        if (TAGOF(goal) == VariableTag)
            Errors.instantiationError();
        compiledCode = Compiler.compileQuery(goal);
        //console.log("Compiled " + PORTRAY(goal) + " to " + util.inspect(compiledCode, {depth:null}));
    }
    catch(e)
    {
        set_exception(env, e);
        return false;
    }
    env.nextFrame.functor = Functor.get(AtomTerm.get("<meta-call>"), 1);
    env.nextFrame.clause = compiledCode.clause;
    env.nextFrame.returnPC = env.PC+1;
    env.nextFrame.choicepoint = env.choicepoints.length;
    env.argP = env.nextFrame.slots;
    env.argI = 0;
    // Now the arguments need to be filled in. This takes a bit of thought.
    // What we have REALLY done is taken Goal and created a new, local predicate '$query'/N like this:
    //     '$query'(A, B, C, ....Z):-
    //           Goal.
    // Where the variables of Goal are A, B, C, ...Z. This implies we must now push the arguments of $query/N
    // onto the stack, and NOT the arguments of Goal, since we are about to 'call' $query.
    //console.log("Vars: " + compiledCode.variables);
    for (var i = 0; i < compiledCode.variables.length; i++)
        env.nextFrame.slots[i] = compiledCode.variables[i];
    return true;
}


function redo_execute(env)
{
    var current_opcode = 0;
//    var d0 = new Date().getTime();
    next_instruction:
    while (!env.halted)
    {
        //env.debug_times[current_opcode] = (env.debug_times[current_opcode] || 0) + new Date().getTime() - d0
        //d0 = new Date().getTime();

        var opcodes = env.currentFrame.clause.opcodes;

        if (next_opcode === undefined && opcodes[env.PC] === undefined)
        {
            console.log(util.inspect(env.currentFrame));
            console.log("Illegal fetch at " + env.PC);
            throw new Error("Illegal fetch");
        }
        current_opcode = (next_opcode || opcodes[env.PC]) | 0;
        next_opcode = undefined;
        //if (env.debugger_steps >= 50) return;
        //env.debugger_steps++;
        //env.debug_ops[current_opcode] = (env.debug_ops[current_opcode] || 0) + 1;
        if (env.debugging == true)
        {
            print_instruction(env, current_opcode);
        }
        switch(current_opcode)
	{
            case 0: // i_fail
            {
                if (backtrack(env))
                    continue;
                //env.debug_times[current_opcode] = (env.debug_times[current_opcode] || 0) + new Date().getTime() - d0
                env.yieldInfo.onFailure();
                return false;
            }
            case 1: // i_enter
            {
                env.nextFrame = new Frame(env);
                env.argI = 0;
                env.argP = env.nextFrame.slots;
                // FIXME: assert(env.argS.length == 0)
                env.PC++;
                continue;
            }
            case 2: // i_exit_query
            {
                env.yieldInfo.onSuccess(env.choicepoints.length != env.yieldInfo.initial_choicepoint_depth);
                //env.debug_times[current_opcode] = (env.debug_times[current_opcode] || 0) + new Date().getTime() - d0
                return;
            }
            case 3: // i_exitcatch
            {
                var slot = ((opcodes[env.PC+1] << 8) | (opcodes[env.PC+2]));
                if (env.currentFrame.reserved_slots[slot] == env.choicepoints.length)
                {
                    // Goal has exited deterministically, we do not need our backtrack point anymore since there is no way we could
                    // backtrack into Goal and generate an exception.
                    // Note that it is possible, if we executed the handler, that the fake choicepoint has already been deleted
                    env.choicepoints.pop();
                }
                next_opcode = LOOKUP_OPCODE["i_exit"].opcode;
                continue next_instruction;
            }
            case 4: // i_foreign
            {
                // Set the foreign info to undefined
                env.currentFrame.reserved_slots[0] = undefined;
                // FALL-THROUGH
            }
            case 5: //i_foreignretry
            {
                env.foreign = env.currentFrame.reserved_slots[0];
                // FIXME: Why do we sometimes get things on the currentFrame for a foreign predicate that are wrong?!
                // I think it can happen if we backtrack to a previous goal - we only create the nextFrame once and then just keep reusing it on i_exit
                // So: foo:- goal_with_choicepoint(A,B,C,D), foreign_predicate.
                // So that explains why slots is the wrong /length/ but not how one of the values in slots[] is undefined
                // The test case is to click the 'top' button on the Demo proactive app and change the next line to var args = env.currentFrame.slots.slice(0)
                var args = [];
                for (var i = 0; i < CTable.get(env.currentFrame.functor).arity; i++)
                {
                    args[i] = DEREF(env.currentFrame.slots[i]);
                }
                var rc = execute_foreign(env, args);
//                if (env.debugging)
//                    console.log("Foreign return value from " + qux + ": " + rc)
                if (rc == false)
                {
                    if (backtrack(env))
                        continue;
                    env.yieldInfo.onFailure();
                    return false;

                }
                else if (rc == true)
                {
/*
                    if (env.debugging)
                    {
                        console.log(new Error().stack);
                        console.log("going to i_exit from executing foreign code:" + env.currentFrame.functor.toString());
                    }
*/
                    next_opcode = LOOKUP_OPCODE["i_exit"].opcode;
                    continue next_instruction;
                }
                else if (rc == "yield")
                {
                    /*
                    if (env.debugging)
                    {
                        console.log("Yielding");
                        console.log(new Error().stack);
                    }
*/
                    return false;
                }
                else if (rc == "exception")
                {
                    next_opcode = LOOKUP_OPCODE["b_throw_foreign"].opcode;
                    continue next_instruction;
                }
            }
            case 6: // i_exit
            {
                if (env.currentFrame === undefined)
                {
                    console.log("The environment has no frame?!");
                    console.log(env);
                    console.log(env.currentFrame);
                    console.log(env.module_map);
                }
		env.PC = env.currentFrame.returnPC;
                env.currentFrame = env.currentFrame.parent;
                env.nextFrame = new Frame(env);
                env.argP = env.nextFrame.slots;
                env.argI = 0;
                continue;
            }
            case 7: // i_exitfact
	    {
		env.PC = env.currentFrame.returnPC;
                env.currentFrame = env.currentFrame.parent;
                env.nextFrame = new Frame(env);
                env.argP = env.nextFrame.slots;
                env.argI = 0;
		continue;
            }
            case 8: // i_depart
	    {
                var functor = env.currentFrame.clause.constants[((opcodes[env.PC+1] << 8) | (opcodes[env.PC+2]))];
                env.nextFrame.functor = functor;
                if (!get_code(env, functor))
                {
                    next_opcode = LOOKUP_OPCODE["b_throw_foreign"].opcode;
                    continue next_instruction;
                }
		env.nextFrame.PC = env.currentFrame.returnPC;
		env.nextFrame.returnPC = env.currentFrame.returnPC;
                env.nextFrame.parent = env.currentFrame.parent;
		env.argP = env.nextFrame.slots;
                env.argI = 0;
                env.currentFrame = env.nextFrame;
                env.currentFrame.choicepoint = env.choicepoints.length;
                env.nextFrame = new Frame(env);
                env.PC = 0; // Start from the beginning of the code in the next frame
                continue;
            }
            case 9: // b_cleanup_choicepoint
            {
                // This just leaves a fake choicepoint (one you cannot backtrack onto) with a .cleanup value set to the cleanup handler
                var c = new Choicepoint(env, -1);
                //console.log("Goal: " + env.argP[env.argI-1]);
                //console.log("Catcher: " + env.argP[env.argI-2]);
                c.cleanup = {goal: env.argP[env.argI-1],
                             catcher: env.argP[env.argI-2]};
                env.argP-=2;
                env.choicepoints.push(c);
                env.PC++;
                continue;
            }

            case 10: // b_throw
            {
                // This is the hardest part of the exception handling process.
                // We have to walk the stack, looking for frames that correspond to calls to catch/3, find the unifier in the frame, and try to unify it with argP[argI-1]
                // If unsuccessful (or partially successful!) we must unwind and look further. If nothing is found and we get to the top, throw a real Javascript exception
                // Finally, care must be taken to unwind any choicepoints we find along the way as we are go so that if the exception is caught we cannot later backtrack
                // into a frame that should've been destroyed by the bubbling exception

                // First, make a copy of the ball, since we are about to start unwinding things and we dont want to undo its binding
                set_exception(env, env.argP[env.argI-1]);
                // Fall-through
            }
            case 11: // b_throw_foreign
            {
                var frame = env.currentFrame;
                //console.log(util.inspect(env.choicepoints));
                //console.log("EXCEPTION: Looking for handler..");
                while (frame != undefined)
                {
                    //console.log(CTable.get(frame.functor).toString());
                    if (frame.functor == Constants.catchFunctor)
                    {
                        // Unwind to the frames choicepoint. Note that this will be set to the fake choicepoint we created in i_catch
                        //console.log("Frame " + frame.functor + " has choicepoint of " + frame.choicepoint);
                        //console.log("Env has " + env.choicepoints.length + " choicepoints");
                        backtrack_to(env, frame.choicepoint);
                        //console.log("There are now " + env.choicepoints.length + " choicepoints left");
                        // Try to unify (a copy of) the exception with the unifier
                        if (env.unify(env.copyTerm(exception), frame.slots[1]))
                        {
                            // Success! Exception is successfully handled. Now we just have to do i_usercall after adjusting the registers to point to the handler
                            // Things get a bit weird here because we are breaking the usual logic flow by ripping the VM out of whatever state it was in and starting
                            // it off in a totally different place. We have to reset argP, argI and PC then pretend the next instruction was i_usercall
                            env.argP = env.currentFrame.slots;
                            env.argI = 3;

                            env.PC = 12; // After we have executed the handler we need to go to the i_exitcatch which should be at PC=12+1
                                         // This is a bit brittle, really :(

                            // One final requriement - the handler is actually executing below the catch/3 frame. If we re-throw the exception in the handler
                            // we will end up right back here, which is not the intention. To fix this, replace the functor of this frame with something else
                            frame.functor = Constants.caughtFunctor;

                            // Javascript doesnt have a goto statement, so this is a bit tricky
                            next_opcode = LOOKUP_OPCODE["i_usercall"].opcode;
                            //console.log("Handling exception:" + frame.slots[1].dereference());
                            continue next_instruction;
                        }
                    }
                    frame = frame.parent;
                }
                if(exception.stack != null)
                    env.yieldInfo.onError(Errors.makeSystemError(AtomTerm.get(exception.toString())));
                else
                    env.yieldInfo.onError(Errors.makeSystemError(exception));
                //env.debug_times[current_opcode] = (env.debug_times[current_opcode] || 0) + new Date().getTime() - d0
                return;
            }
            case 12: // i_switch_module
            {
                // FIXME: We must make sure that when processing b_throw that we pop modules as needed
                // FIXME: There could also be implications for the cleanup in setup-call-cleanup?
                var module = env.currentFrame.clause.constants[((opcodes[env.PC+1] << 8) | (opcodes[env.PC+2]))];
                env.currentModule = env.getModule(CTable.get(module).value);
                env.currentFrame.contextModule = env.currentModule;
                //console.log(env.currentModule);
                env.PC+=3;
                continue;
            }
            case 13: // i_exitmodule
            {
                env.PC++;
                continue;
            }
            case 14: // i_call
            {
                var functor = env.currentFrame.clause.constants[((opcodes[env.PC+1] << 8) | (opcodes[env.PC+2]))];
                env.nextFrame.functor = functor;
                if (functor === undefined)
                {
                    console.log("Current goal is " + CTable.get(env.currentFrame.functor).toString() + " hTOP: " + HTOP);
                    console.log(util.inspect(env.currentFrame.clause, {depth: null}));
                    console.log(env.currentFrame.clause.constants + " slot " + (opcodes[env.PC+1] << 8) + "," + opcodes[env.PC+2]);
                }
                if (!get_code(env, functor))
                {
                    next_opcode = LOOKUP_OPCODE["b_throw_foreign"].opcode;
                    continue next_instruction;
                }
                env.nextFrame.returnPC = env.PC+3;
                env.currentFrame = env.nextFrame;
                env.currentFrame.choicepoint = env.choicepoints.length;
                // Reset argI to be at the start of the current array. argP SHOULD already be slots for the next frame since we were
                // just filling it in
                // FIXME: assert(env.argS.length === 0)
                // FIXME: assert(env.argP === env.currentFrame.slots)
                env.argI = 0;
                env.nextFrame = new Frame(env);
                env.PC = 0; // Start from the beginning of the code in the next frame
                continue;
            }
            case 15: // i_catch
            {
                var slot = ((opcodes[env.PC+1] << 8) | (opcodes[env.PC+2]));
                // i_catch must do a few things:
                //    * Create a backtrack point at the start of the frame so we can undo anything in the guarded goal if something goes wrong
                //    * ensure argP[argI-1] points to the goal
                //    * Jump to i_usercall to call the goal
                env.choicepoints.push(new Choicepoint(env, -1));
                //console.log("Created fake choicepoint");
                //console.log("slots contains " + env.currentFrame.slots.length);
                // Since catch/3 can never itself contain a cut (the bytecode is fixed at i_catch, i_exitcatch), we can afford to tweak the frame a bit
                // by moving the frames cut choicepoint to the fake choicepoint we have just created, we can simplify b_throw
                env.currentFrame.choicepoint = env.choicepoints.length;
                env.currentFrame.reserved_slots[slot] = env.choicepoints.length;
                //console.log(env.currentFrame.slots);
                // Currently argP points to the first slot in the next frame that we have started building since we've done i_enter already
                // we need to set it back to the slots of the /current/ frame so that i_usercall finds the goal
                env.argP = env.currentFrame.slots;
                env.argI = 1;
                env.PC+=2; // i_usercall will add the third byte in a moment
                // Fall through to i_usercall
            }
            case 16: // i_usercall
            {
                // argP[argI-1] is a goal that we want to execute. First, we must compile it
                env.argI--;
                var goal = DEREF(env.argP[env.argI]);
                //console.log("Calling: " + PORTRAY(goal));
                if (!usercall(env, goal))
                {
                    next_opcode = LOOKUP_OPCODE["b_throw_foreign"].opcode;
                    continue next_instruction;
                }
                env.currentFrame = env.nextFrame;
                env.nextFrame = new Frame(env);
                env.PC = 0;
                continue;
            }
            case 17: // i_cut
            {
                // The task of i_cut is to pop and discard all choicepoints newer than env.currentFrame.choicepoint
                //console.log("Cutting choicepoints to " + env.currentFrame.choicepoint);
                // If we want to support cleanup, we cannot just do this:
                //env.choicepoints = env.choicepoints.slice(0, env.currentFrame.choicepoint);
                //console.log("Cut to " + env.currentFrame.choicepoint);
                if (!cut_to(env, env.currentFrame.choicepoint))
                    return false;
                env.currentFrame.choicepoint = env.choicepoints.length;
                env.PC++;
                //console.log("Choicepoints after cut: " + env.choicepoints.length);
                continue;
            }
            case 18: // c_cut
            {
                // The task of c_cut is to pop and discard all choicepoints newer than the value in the given slot
                var slot = ((opcodes[env.PC+1] << 8) | (opcodes[env.PC+2]));
                if (!cut_to(env, env.currentFrame.reserved_slots[slot]))
                    return false;
                env.PC+=3;
                continue;
            }
            case 19: // c_lcut
            {
                // The task of c_lcut is to pop and discard all choicepoints newer than /one greater than/ the value in the given slot
                var slot = ((opcodes[env.PC+1] << 8) | (opcodes[env.PC+2]));
                if (!cut_to(env, env.currentFrame.reserved_slots[slot]+1))
                    return false;
                env.PC+=3;
                continue;
            }
            case 20: // c_ifthen
            {
                var slot = ((opcodes[env.PC+1] << 8) | (opcodes[env.PC+2]));
                env.currentFrame.reserved_slots[slot] = env.choicepoints.length;
                env.PC+=3;
                continue;
            }
            case 21: // c_ifthenelse
            {
                var slot = ((opcodes[env.PC+1] << 8) | (opcodes[env.PC+2]));
                var address = ((opcodes[env.PC+3] << 24) | (opcodes[env.PC+4] << 16) | (opcodes[env.PC+5] << 8) | (opcodes[env.PC+6] << 0)) + env.PC;
                env.currentFrame.reserved_slots[slot] = env.choicepoints.length;
                env.choicepoints.push(new Choicepoint(env, address));
                env.PC+=7;
                continue;
            }

            case 22: // i_unify
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
                //console.log("iUnify: Failed to unify " + util.inspect(arg1) + " and " + util.inspect(arg2));
                if (backtrack(env))
                    continue;
                env.yieldInfo.onFailure();
                return false;

            }
            case 23: // b_firstvar
            {
                var slot = ((opcodes[env.PC+1] << 8) | (opcodes[env.PC+2]));
                env.currentFrame.slots[slot] = MAKEVAR();
                //console.log("firstvar: setting frame value at " + env.argI + " to " + PORTRAY(env.currentFrame.slots[slot]));
                env.argP[env.argI++] = link(env, env.currentFrame.slots[slot]);
                env.PC+=3;
                continue;
            }
            case 24: // b_argvar
            {
                var slot = ((opcodes[env.PC+1] << 8) | (opcodes[env.PC+2]));
                //console.log("argvar: getting variable from slot " + slot + ": " + env.currentFrame.slots[slot]);
                var arg = DEREF(env.currentFrame.slots[slot]);
                //console.log("Value of variable is: " + util.inspect(arg, {showHidden: false, depth: null}));
                if (TAGOF(arg) == VariableTag)
                {
                    // FIXME: This MAY require trailing
                    env.argP[env.argI] = MAKEVAR();
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
            case 25: // b_var
            {
                var slot = ((opcodes[env.PC+1] << 8) | (opcodes[env.PC+2]));
                // It MAY be that the variable is not yet initialized. This can happen if we have code like
                // (foo(X) ; true), bar(X).
                // which compiles to
                //        c_or(else), b_firstvar(0), i_call(foo), c_jump(end),
                // else:
                // end:   b_var(0), i_call(bar)
                //
                // The status of X depends on which branch in the disjunction we took
                if (env.currentFrame.slots[slot] == undefined)
                {
                    //console.log("Had to make a new var...");
                    env.currentFrame.slots[slot] = MAKEVAR();
                }
                //console.log("Just pushed: " + env.currentFrame.slots[slot] + "-> " + PORTRAY(env.currentFrame.slots[slot]) + " to next frame slot " + env.argI);
                env.argP[env.argI] = link(env, env.currentFrame.slots[slot]);
                //console.log("Checking: " + PORTRAY(env.argP[env.argI]));
                env.argI++;
                env.PC+=3;
                continue;
            }
            case 26: // b_pop
            {
		popArgFrame(env);
                env.PC++;
                continue;
            }
            case 27: // b_atom
	    {
                var index = ((opcodes[env.PC+1] << 8) | (opcodes[env.PC+2]));
		env.argP[env.argI++] = env.currentFrame.clause.constants[index];
		env.PC+=3;
                continue;
            }
            case 28: // b_void
	    {
		var index = ((opcodes[env.PC+1] << 8) | (opcodes[env.PC+2]));
                env.argP[env.argI++] = MAKEVAR();
                env.PC++;
                continue;
            }
            case 29: // b_functor
            {
		var index = ((opcodes[env.PC+1] << 8) | (opcodes[env.PC+2]));
                var functor = env.currentFrame.clause.constants[index];
                var functor_object = CTable.get(functor);
                var new_term_args = new Array(functor_object.arity);
                for (var i = 0; i < new_term_args.length; i++)
                    new_term_args[i] = 0;
                var H = HTOP+1;
                //console.log(new_term_args + " from " + functor_object.arity);
		env.argP[env.argI++] = CompoundTerm.create(functor, new_term_args);
		newArgFrame(env);
                env.argP = HEAP;
                env.argI = H;
		env.PC+=3;
		continue;
            }
            case 30: // h_firstvar
            {
                // varFrame(FR, *PC++) in SWI-Prolog is the same as
		// env.currentFrame.slot[(opcodes[env.PC+1] << 8) | (opcodes[env.PC+2])] in PS2
                // basically varFrameP(FR, n) is (((Word)f) + n), which is to say it is a pointer to the nth word in the frame
                // In PS2 parlance, these are 'slots'
		var slot = ((opcodes[env.PC+1] << 8) | (opcodes[env.PC+2]));
                if (env.mode == "write")
		{
		    // If in write mode, we must create a variable in the current frame's slot, then bind it to the next arg to be matched
                    // This happens if we are trying to match a functor to a variable. To do that, we create a new term with the right functor
                    // but all the args as variables, pushed the current state to argS, then continue matching with argP pointing to the args
                    // of the functor we just created. In this particular instance, we want to match one of the args (specifically, argI) to
                    // a variable occurring in the head, for example the X in foo(a(X)):- ...
                    // the compiler has allocated an appropriate slot in the frame above the arity of the goal we are executing
                    // we just need to create a variable in that slot, then bind it to the variable in the current frame
                    env.currentFrame.slots[slot] = MAKEVAR();
                    env.bind(env.argP[env.argI], env.currentFrame.slots[slot]);
		}
                else
                {
                    // in read mode, we are trying to match an argument to a variable occurring in the head. If the thing we are trying to
                    // match is not a variable, put a new variable in the slot and bind it to the thing we actually want
                    // otherwise, we can just copy the variable directly into the slot
                    if (TAGOF(env.argP[env.argI]) == VariableTag)
                    {
                        env.currentFrame.slots[slot] = link(env, env.argP[env.argI]);
                    }
                    else
                    {
                        env.currentFrame.slots[slot] = env.argP[env.argI];
                    }
                }
                // H_FIRSTVAR always succeeds - so move on to match the next cell, and increment PC for the next instructions
                env.argI++;
                env.PC+=3;
                continue;
            }
            case 31: // h_functor
            {
                var functor = env.currentFrame.clause.constants[((opcodes[env.PC+1] << 8) | (opcodes[env.PC+2]))];
                var arg = DEREF(env.argP[env.argI++]);
                env.PC+=3;
                // Try to match a functor
                if (TAGOF(arg) == CompoundTag)
                {
                    if (FUNCTOROF(arg) == functor)
		    {
			newArgFrame(env);
                        env.argP = HEAP;
                        env.argI = ARGPOF(arg, 0);
                        continue;
                    }
                }
                else if (TAGOF(arg) == VariableTag)
                {
		    newArgFrame(env);
                    var args = new Array(CTable.get(functor).arity);
                    for (var i = 0; i < args.length; i++)
                        args[i] = MAKEVAR();
                    var H = HTOP+1;
                    env.bind(arg, CompoundTerm.create(functor, args));
                    env.argP = HEAP;
                    env.argI = H;
                    env.mode = "write";
                    continue;
                }
                //console.log("argP: " + util.inspect(env.argP));
                //console.log("Failed to unify " + PORTRAY(arg) + " with the functor " + CTable.get(functor).toString());

                if (backtrack(env))
                    continue;
                env.yieldInfo.onFailure();
                return false;
            }
            case 32: // h_pop
            {
                popArgFrame(env);
                env.PC++;
                continue;
            }
            case 33: // h_atom
            {
		var atom = env.currentFrame.clause.constants[((opcodes[env.PC+1] << 8) | (opcodes[env.PC+2]))];
                var arg = DEREF(env.argP[env.argI++]);
                env.PC+=3;
                if (arg == atom)
                    continue;
                if (TAGOF(arg) == VariableTag)
                {
                    env.bind(arg, atom);
                }
                else
                {
                    //console.log("argP: " + util.inspect(env.argP, {showHidden: false, depth:null}));
                    //console.log("argI: " + env.argI);
                    //console.log("Failed to unify " + util.inspect(arg) + " with the atom " + util.inspect(atom));
                    if (backtrack(env))
                        continue;
                    env.yieldInfo.onFailure();
                    return false;
                }
                continue;
            }
            case 34: // h_void
            {
                env.argI++;
	        env.PC++;
	        continue;
            }
            case 35: // h_var
            {
                var slot = ((opcodes[env.PC+1] << 8) | (opcodes[env.PC+2]));
                var arg = DEREF(env.argP[env.argI++]);
                //console.log("h_var from slot " + slot + ": " +arg+", and " + env.currentFrame.slots[slot])
                if (!env.unify(arg, env.currentFrame.slots[slot]))
                {
                    if (backtrack(env))
                        continue;
                    env.yieldInfo.onFailure();
                    return false;
                }
                env.PC+=3;
		continue;
	    }
            case 36: // c_jump
            {
                env.PC +=      (opcodes[env.PC+1] << 24) | (opcodes[env.PC+2] << 16) | (opcodes[env.PC+3] << 8) | (opcodes[env.PC+4] << 0);
		continue;
	    }
            case 37: // c_or
            {
                var address = ((opcodes[env.PC+1] << 24) | (opcodes[env.PC+2] << 16) | (opcodes[env.PC+3] << 8) | (opcodes[env.PC+4] << 0)) + env.PC;
                env.choicepoints.push(new Choicepoint(env, address));
                env.PC+=5;
                continue;
            }
            case 38: // try_me_or_next_clause
            {
                env.choicepoints.push(new ClauseChoicepoint(env));
                env.PC++;
                continue;
            }
            case 39: // trust_me
            {
                env.PC++;
                continue;
            }
            case 40: // s_qualify
            {
                var slot = ((opcodes[env.PC+1] << 8) | (opcodes[env.PC+2]));
                var value = DEREF(env.currentFrame.slots[slot]);
                //console.log("Qualifying " + value + " or " + PORTRAY(value) + " from slot " + slot );
                if (!(TAGOF(value) == CompoundTag && FUNCTOROF(value) == Constants.crossModuleCallFunctor))
                    env.currentFrame.slots[slot] = CompoundTerm.create(Constants.crossModuleCallFunctor, [env.currentFrame.parent.contextModule.term, value]);
                env.PC+=3;
                //console.log("Arg is now: " + PORTRAY(env.currentFrame.slots[slot]));
                continue;
            }
            default:
	    {
                throw new Error("illegal instruction: " + Opcodes[opcodes[env.PC]].label);
            }
        }
    }
}

function resume(env, success)
{
    if (success == false)
    {
        if (backtrack(env))
            redo_execute(env);
        else
            env.yieldInfo.onFailure();
    }
    if (success == true)
    {
        next_opcode = LOOKUP_OPCODE["i_exit"].opcode;
        redo_execute(env);
    }
    else
    {
        set_exception(env, success);
        next_opcode = LOOKUP_OPCODE["b_throw_foreign"].opcode;
        redo_execute(env);
    }
}

module.exports = {execute: execute,
                  resume: resume,
                  backtrack: backtrack};
