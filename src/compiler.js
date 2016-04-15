var Constants = require('./constants.js');
var CompoundTerm = require('./compound_term.js');
var VariableTerm = require('./variable_term.js');
var AtomTerm = require('./atom_term.js');
var IntegerTerm = require('./integer_term.js');
var Functor = require('./functor.js');


function dereference_recursive(term)
{
    var term = term.dereference();
    if (term instanceof CompoundTerm)
    {
	var new_args = [];
	for (var i = 0; i < term.args.length; i++)
	    new_args[i] = dereference_recursive(term.args[i])
	return new CompoundTerm(term.functor, new_args);
    }
    return term;
}

var Instructions =
    {
	iFail: "i_fail",
	iAllocate: "i_allocate",
	iEnter: "i_enter",
	iSaveCut: "i_save_cut",
	iExit: "i_exit",
	iExitFact: "i_exitfact",
	iCall: "i_call",
	iDepart: "i_depart",
	iCut: "i_cut",

	iUnify: "i_unify",
	bFirstVar: "b_firstvar",
	bArgVar: "b_argvar",
	bVar: "b_var",
	bPop: "b_pop",
	bConstant: "b_constant",
	bFunctor: "b_functor",

	hConstant: "h_constant",
	hFirstVar: "h_firstvar",
	hFunctor: "h_functor",
	hPop: "h_pop",
	hVoid: "h_void",
	hVar: "h_var",


	tryMeElse: "try_me_else",
	retryMeElse: "retry_me_else",
	trustMe: "trust_me",

	nop: "nop"
    };

function compilePredicate(clauses)
{
    var instructions = [];
    if (clauses == [])
    {
	instructions.push({opcode: Instructions.iFail});
    }
    else
    {
	var clausePointers = [];
	for (var i = 0; i < clauses.length; i++)
	{
	    // Save room for the try-retry-trust construct
	    if (clauses.length > 1)
	    {
		instructions.push({opcode: Instructions.nop});
		clausePointers[i] = instructions.length;
	    }
	    compileClause(dereference_recursive(clauses[i]), instructions);
	}
	// Now go back and fill in the try-retry-trust constructs
	if (clauses.length > 1)
	{
	    for (var i = 0; i < clauses.length-1; i++)
	    {
		instructions[clausePointers[i]] = {opcode: (i == 0)?(Instructions.tryMeElse):(Instructions.retryMeElse),
						   retry: clausePointers[i+1]}
	    }
	    instructions[clausePointers[clauses.length-1]] = {opcode: Instructions.trustMe};
	}
    }

    // Now convert instructions into an array of primitive types and an array of constants
    return assemble(instructions);
}

function assemble(instructions)
{
    return {instructions: instructions,
	    constants: []};
}

function compileClause(term, instructions)
{
    // First, reserve some slots for !/0, ->/2 and catch/3
    var reserved = 0;
    var context = {isFirstGoal:true,
		   hasGlobalCut:false};
    if (term.functor == Constants.clauseFunctor)
    {
	// A clause
	reserved += getReservedEnvironmentSlots(term.args[1], context);
	// Finally, we need a slot for each variable, since this is to be a stack-based, rather than register-based machine
	// If it were register-based, we would need a register for each variable instead
	// FIXME: By arranging these more carefully we might do better for LCO
	var envSize = reserved;
	var variables = {};
	var context = {nextSlot:0};
	envSize += analyzeVariables(term.args[0], true, 0, variables, context);
	envSize += analyzeVariables(term.args[1], false, 1, variables, context);
	console.log(variables);
//	if (envSize != 0)
//	    instructions.push({opcode: Instructions.iAllocate, envSize:envSize, reserved:reserved});
	if (context.hasGlobalCut)
	    instructions.push({opcode: Instructions.iSaveCut,
			       slot: 0});
	compileHead(term.args[0], variables, instructions);
	instructions.push({opcode: Instructions.iEnter});
	compileBody(term.args[1], variables, instructions, true);
	instructions.push({opcode: Instructions.iExit});
    }
    else
    {
	// Fact
	console.log(term.functor);
	console.log(Constants.clauseFunctor);
	// FIXME: A fact CAN have important variables in it. Like foo(A, A) should not succeed if the only clause is foo(a, b).
	compileHead(term, {}, instructions);
	instructions.push({opcode: Instructions.iExitFact});
    }
}

function compileHead(term, variables, instructions)
{
    if (term instanceof CompoundTerm)
    {
	for (var i = 0; i < term.functor.arity; i++)
	{
	    compileArgument(term.args[i], variables, instructions, false);
	}
    }
}

function compileArgument(arg, variables, instructions, embeddedInTerm)
{
    if (arg instanceof VariableTerm)
    {
	if (variables[arg.name].fresh)
	{
	    variables[arg.name].fresh = false;
	    if (embeddedInTerm)
	    {
		instructions.push({opcode: Instructions.hFirstVar,
				   name: arg.name,
				   slot:variables[arg.name].slot});
	    }
	    else
	    {
		instructions.push({opcode: Instructions.hVoid});
	    }
	}
	else
	    instructions.push({opcode: Instructions.hVar,
			       slot: variables[arg.name].slot});
    }
    else if (arg instanceof AtomTerm)
    {
	instructions.push({opcode: Instructions.hConstant,
			   atom: arg});
    }
    else if (arg instanceof IntegerTerm)
    {
	instructions.push({opcode: Instructions.hConstant,
			   integer: arg});
    }
    else if (arg instanceof CompoundTerm)
    {
	instructions.push({opcode: Instructions.hFunctor,
			   functor: arg.functor});
	for (var i = 0; i < arg.functor.arity; i++)
	    compileArgument(arg.args[i], variables, instructions, true);
	instructions.push({opcode: Instructions.hPop});
    }
}

function compileBody(term, variables, instructions, isTailGoal)
{
    if (term instanceof VariableTerm)
    {
	compileTermCreation(term, variables, instructions);
	instructions.push({opcode: isTailGoal?Instructions.iDepart:Instructions.iCall,
			   functor: Functor.get(AtomTerm.get("call"), 1)});
    }
    else if (term == Constants.cutAtom)
    {
	instructions.push({opcode: Instructions.iCut});
    }
    else if (term == Constants.trueAtom)
    {
	instructions.push({opcode: Instructions.iTrue});
    }
    else if (term == Constants.failAtom)
    {
	instructions.push({opcode: Instructions.iFail});
    }
    else if (term instanceof AtomTerm)
    {
	instructions.push({opcode: isTailGoal?Instructions.iDepart:Instructions.iCall,
			   functor: Functor.get(term, 0)});
    }
    else if (term instanceof CompoundTerm)
    {
	if (term.functor == Constants.conjunctionFunctor)
	{
	    compileBody(term.args[0], variables, instructions, false);
	    compileBody(term.args[1], variables, instructions, isTailGoal);
	}
	else if (term.functor == Constants.disjunctionFunctor)
	{
	    var currentInstruction = instructions.length;
	    // Save space for a try-me-else here
	    instructions.push({opcode: Instructions.nop});
	    compileBody(term.args[0], variables, instructions, isTailGoal);
	    instructions[currentInstruction] = {opcode: Instructions.tryMeElse,
						retry: instructions.length};
	    instructions.push({opcode: Instructions.trustMe});
	    compileBody(term.args[1], variables, instructions, isTailGoal);
	}
	else if (term.functor == Constants.throwFunctor)
	{
	    compileTermCreation(term.args[0], variables, instructions);
	    instructions.push({opcode: Instructions.iThrow});
	}
	else if (term.functor == Constants.localCutFunctor)
	{
	    // FIXME: Implement
	}
	else if (term.functor == Constants.catchFunctor)
	{
	    // FIXME: Implement
	}
	else if (term.functor == Constants.unifyFunctor)
	{
	    compileTermCreation(term.args[0], variables, instructions);
	    compileTermCreation(term.args[1], variables, instructions);
	    instructions.push({opcode: Instructions.iUnify});
	}
	else
	{
	    for (var i = 0; i < term.functor.arity; i++)
		compileTermCreation(term.args[i], variables, instructions);
	    instructions.push({opcode: isTailGoal?Instructions.iDepart:Instructions.iCall,
			       functor: term.functor});
	}
    }
    else
    {
	// FIXME: This should be a type error
    }
}

function compileTermCreation(term, variables, instructions)
{
    if (term instanceof VariableTerm)
    {
	if (variables[term.name].fresh)
	{
	    instructions.push({opcode: Instructions.bFirstVar,
			       name:term.name,
			       slot:variables[term.name].slot});
	    variables[term.name].fresh = false;
	}
	else if (variables[term.name].isArg)
	{
	    instructions.push({opcode: Instructions.bArgVar,
			       name:term.name,
			       slot:variables[term.name].slot});
	}
	else
	{
	    instructions.push({opcode: Instructions.bVar,
			       name:term.name,
			       slot:variables[term.name].slot});

	}
    }
    else if (term instanceof CompoundTerm)
    {
	instructions.push({opcode: Instructions.bFunctor,
			   functor:term.functor});
	for (var i = 0; i < term.functor.arity; i++)
	    compileTermCreation(term.args[i], variables, instructions);
	instructions.push({opcode: Instructions.bPop});
    }
    else // then it must be a constant
    {
	instructions.push({opcode: Instructions.bConstant,
			   constant:term});

    }
}


// Here we reserve extra slots in the environment for two purposes:
//   1) Cuts - each local cut appearing in the body must have its own slot so we can save the value of B0
//      this is because when executing (foo -> bar) the act of calling foo/0 can change B0 (indeed, it will set it to B on entry)
//   2) Exceptions - each catch/3 term needs two slots in the environment: One for the associated cut, and one for the ball
// We have to pull apart any goals that we compile away and add slots for their arguments as well since they will be flattened into the
// body itself. This includes ,/2, ;/2, ->/2 and catch/3
function getReservedEnvironmentSlots(term, context)
{
    var slots = 0;
    if (term instanceof CompoundTerm)
    {
	if (term.functor === Constants.conjunctionFunctor)
	{
	    slots += getReservedEnvironmentSlots(term.args[0], context);
	    context.isFirstGoal = false;
	    slots += getReservedEnvironmentSlots(term.args[1], context);
	}
	else if (term.functor === Constants.disjunctionFunctor)
	{
	    context.isFirstGoal = false
	    slots += getReservedEnvironmentSlots(term.args[0], context);
	    slots += getReservedEnvironmentSlots(term.args[1], context);
	}
	else if (term.functor === Constants.localCutFunctor)
	{
	    slots++;
	    context.isFirstGoal = false;
	    slots += getReservedEnvironmentSlots(term.args[0], context);
	    slots += getReservedEnvironmentSlots(term.args[1], context);
	}
	else if (term.functor === Constants.catchFunctor)
	{
	    context.isFirstGoal = false;
	    slots += 2;
	    // FIXME: What about args 0 and 2?
	}
    }
    else if (term === Constants.cutAtom && context.isFirstGoal && !context.hasGlobalCut)
    {
	slots++;
	context.hasGlobalCut = true;
    }
    return slots;
}

// analyzeVariables builds a map of all the variables and returns the number of variables encountered
// SWI-Prolog allocates a table of arity variables on the argument stack, for some reason
// If an arg is not a variable, then that position cannot be used since it contains something other than a variable
// For example, if the head is foo(A, q(B), x, A)
// Then the first 4 slots in the frame will be:
// 0: A
// 1: <STR, n>   where n holds <q/1> and n+1 holds B
// 2: x
// 3: A again
// 4+: Local variables, including B.
function analyzeVariables(term, isHead, depth, map, context)
{
    rc = 0;
    if (term instanceof VariableTerm)
    {
	if (map[term.name] === undefined)
	{
	    map[term.name] = ({variable: term,
			       isArg: isHead,
			       fresh: true,
			       slot: context.nextSlot});
	    context.nextSlot++;
	    rc++;
	}
    }
    else if (term instanceof CompoundTerm)
    {
	if (isHead && depth == 0)
	{
	    // Reserve the first arity slots since that is where the previous
	    // frame is going to write the arguments
	    for (var i = 0; i < term.functor.arity; i++)
	    {
		if (term.args[i] instanceof VariableTerm)
		{
		    if (map[term.args[i].name] === undefined)
		    {
			map[term.args[i].name] = ({variable: term.args[i],
						   isArg: isHead,
						   fresh: true,
						   slot: context.nextSlot});
			rc++;
		    }
		}
		context.nextSlot++;
	    }
	}
	for (var i = 0; i < term.functor.arity; i++)
	    rc += analyzeVariables(term.args[i], isHead, depth+1, map, context);
    }
    return rc;
}

module.exports = compilePredicate;
