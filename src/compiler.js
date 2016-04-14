var Constants = require('./constants.js');
var CompoundTerm = require('./compound_term.js');
var VariableTerm = require('./variable_term.js');
var AtomTerm = require('./atom_term.js');


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
	iSaveCut: "i_save_cut",
	iExit: "i_exit",
	iExitFact: "i_exitfact",
	iCall: "i_call",

	iUnify: "i_unify",
	iPushArgument: "i_pusharg",
	iPushConstant: "i_pushconst",
	iPushFunctor: "i_pushfunctor",

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
	var envSize = reserved;
	var variables = [];
	//    envSize += analyzeVariables(term, permanentVariables);
	if (reserved != 0)
	    instructions.push({opcode: Instructions.iAllocate, envSize:envSize, reserved:reserved});
	if (context.hasGlobalCut)
	    instructions.push({opcode: Instructions.iSaveCut,
			       slot: 0});
	compileHead(term.args[0], variables, instructions);
	compileBody(term.args[1], variables, instructions);
	instructions.push({opcode: Instructions.iExit});
    }
    else
    {
	// Fact
	console.log(term.functor);
	console.log(Constants.clauseFunctor);
	// FIXME: A fact CAN have important variables in it. Like foo(A, A) should not succeed if the only clause is foo(a, b).
	compileHead(term, [], instructions);
	instructions.push({opcode: Instructions.iExitFact});
    }
}

function compileHead(term, variables, instructions)
{
    if (term instanceof CompoundTerm)
    {
	for (var i = 0; i < term.functor.arity; i++)
	{
	    instructions.push({opcode: Instructions.iPushArgument,
			       index: i});
	    compileTermCreation(term.args[i], variables, instructions);
	    instructions.push({opcode: Instructions.iUnify});
	}
    }
    else
    {
	// FIXME: I dont think we actually need to do anything here?
    }
}

function compileBody(term, variables, instructions)
{
    if (term instanceof VariableTerm)
    {
	compileTermCreation(term, variables, instructions);
	instructions.push({opcode: Instructions.iCall,
			   functor: Functor.get(AtomTerm.get("call"), 1)});
    }
    else if (term == Constants.cutAtom)
    {
	instructions.push({opcode: Instructions.iCut,
			   cut_to: "FIXME"});
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
	instructions.push({opcode: Instructions.iCall,
			   functor: Functor.get(term, 0)});
    }
    else if (term instanceof CompoundTerm)
    {
	if (term.functor == Constants.conjunctionFunctor)
	{
	    compileBody(term.args[0], variables, instructions);
	    compileBody(term.args[1], variables, instructions);
	}
	else if (term.functor == Constants.disjunctionFunctor)
	{
	    var currentInstruction = instructions.length;
	    // Save space for a try-me-else here
	    instructions.push({opcode: Instructions.nop});
	    compileBody(term.args[0], variables, instructions);
	    instructions[currentInstruction] = {opcode: Instructions.tryMeElse,
						retry: instructions.length};
	    instructions.push({opcode: Instructions.trustMe});
	    compileBody(term.args[1], variables, instructions);
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
	    instructions.push({opcode: Instructions.iCall,
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
	instructions.push({opcode: Instructions.iPush,
			   slot:variables.indexOf(term)});
    }
    else if (term instanceof CompoundTerm)
    {
	for (var i = 0; i < term.functor.arity; i++)
	    compileTermCreation(term.args[i], variables, instructions);
	instructions.push({opcode: Instructions.iPushFunctor,
			   functor:term.functor});
    }
    else // then it must be a constant
    {
	instructions.push({opcode: Instructions.iPushConstant,
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

// analyzeVariables builds a map of all the variables and returns
function analyzeVariables(term, map)
{
    if (term instanceof VariableTerm)
    {

    }
}

module.exports = compilePredicate;
