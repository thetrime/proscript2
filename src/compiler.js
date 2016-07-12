var Constants = require('./constants.js');
var CompoundTerm = require('./compound_term.js');
var VariableTerm = require('./variable_term.js');
var AtomTerm = require('./atom_term.js');
var BlobTerm = require('./blob_term.js');
var FloatTerm = require('./float_term.js');
var BigIntegerTerm = require('./biginteger_term.js');
var IntegerTerm = require('./integer_term.js');
var Functor = require('./functor.js');
var Errors = require('./errors.js');
var Instructions = require('./opcodes.js').opcode_map;
var util = require('util');


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
                clausePointers[i] = instructions.length;
                instructions.push({opcode: Instructions.nop});
            }
            //console.log("Compiling " + util.inspect(clauses[i], {showHidden: false, depth: null}));
	    compileClause(dereference_recursive(clauses[i]), instructions);
	}
	// Now go back and fill in the try-retry-trust constructs
	if (clauses.length > 1)
	{
	    for (var i = 0; i < clauses.length-1; i++)
	    {
		instructions[clausePointers[i]] = {opcode: (i == 0)?(Instructions.tryMeElse):(Instructions.retryMeElse),
						   address: clausePointers[i+1]}
	    }
	    instructions[clausePointers[clauses.length-1]] = {opcode: Instructions.trustMe};
	}
    }

    // Now convert instructions into an array of primitive types and an array of constants
    var result = assemble(instructions);
    //console.log(util.inspect(result, {showHidden: false, depth: null}));
    return result;
}

function assemble(instructions)
{
    var size = 0;
    var constant_count = 0;
    for (var i = 0; i < instructions.length; i++)
	size += instructionSize(instructions[i].opcode);
    var bytes = new Uint8Array(size);
    var currentInstruction = 0;
    var constants = [];
    for (var i = 0; i < instructions.length; i++)
    {
	currentInstruction += assembleInstruction(instructions, i, bytes, currentInstruction, constants);
    }
    //console.log("Compiled: " + util.inspect(instructions, {showHidden: false, depth: null}) + " to " + bytes + " with constants: " + util.inspect(constants, {showHidden: false, depth: null}));
    return {bytecode: bytes,
	    constants: constants,
            instructions:instructions};
}

function instructionSize(i)
{
    var rc = 1;
    for (var t = 0; t < i.args.length; t++)
    {
	if (i.args[t] == "functor")
	    rc +=2;
	else if (i.args[t] == "atom")
	    rc += 2;
	else if (i.args[t] == "integer")
	    rc += 2;
	else if (i.args[t] == "address")
	    rc += 4;
	else if (i.args[t] == "slot")
            rc += 2;
        else if (i.args[t] == "foreign_function")
            rc += 0;
    }
    return rc;
}

function assembleInstruction(instructions, iP, bytecode, ptr, constants)
{
    var rc = 1;
    var instruction = instructions[iP];
    bytecode[ptr] = instruction.opcode.opcode;
    for (var i = 0; i < instruction.opcode.args.length; i++)
    {
	var arg = instruction.opcode.args[i];
	if (arg == "functor")
	{
	    rc+=2;
	    var index = constants.indexOf(instruction.functor);
	    if (index == -1)
		index = constants.push(instruction.functor) - 1;
	    bytecode[ptr+1] = (index >> 8) & 255
	    bytecode[ptr+2] = (index >> 0) & 255
	}
	else if (arg == "atom")
	{
	    var index = constants.indexOf(instruction.atom);
	    if (index == -1)
		index = constants.push(instruction.atom) - 1;
	    bytecode[ptr+1] = (index >> 8) & 255
	    bytecode[ptr+2] = (index >> 0) & 255
	    rc += 2;
	}
	else if (arg == "address")
        {
            var target = instruction.address;
            var offset = 0;
            for (var t = iP; t < target; t++)
                offset += instructionSize(instructions[t].opcode);
            bytecode[ptr+1] = (offset >> 24) & 0xff;
            bytecode[ptr+2] = (offset >> 16) & 0xff;
            bytecode[ptr+3] = (offset >> 8) & 0xff;
            bytecode[ptr+4] = (offset >> 0) & 0xff;
	    rc += 4;
	}
	else if (arg == "slot")
	{
	    bytecode[ptr+1] = (instruction.slot >> 8) & 255;
	    bytecode[ptr+2] = (instruction.slot >> 0) & 255;
	    rc += 2;
        }
        else if (arg == "foreign_function")
        {
            constants.push(instruction.foreign_function);
        }
    }
    return rc;
}


function compileQuery(term)
{
    var instructions = [];
    // We have to make a fake head with all the variables in the term. Otherwise, when we execute the body, we will get
    // b_firstvar, and the variable passed in will be destroyed
    var variables = [];
    findVariables(term, variables);
    //console.log("Variables found in " + term + ":" + util.inspect(variables));
    compileClause(new CompoundTerm(Constants.clauseFunctor, [new CompoundTerm("$query", variables), dereference_recursive(term)]), instructions);
    var code = assemble(instructions);
    code.variables = variables;
    //console.log(util.inspect(code, {showHidden: false, depth: null}));
    return code;
}

function findVariables(term, variables)
{
    term = term.dereference();
    if (term instanceof VariableTerm && variables.indexOf(term) == -1)
        variables.push(term);
    else if (term instanceof CompoundTerm)
    {
        for (var i = 0; i < term.args.length; i++)
            findVariables(term.args[i], variables);
    }
}


/*
  A frame has 3 kinds of slots:
  ___/ Args: One slot for each argument
 /   \ Vars: One slot for each variable that does not first appear as a term in the args. For
 |     Reserved: This is used for system stuff like cuts, exceptions, etc.
These two go into a single array called 'slots'. The Reserved slots actually go into a different array

The Vars slots bears some explanation. For example in foo(a(A), A), we must allocate a var slot for A, but in foo(A, a(A)) we do NOT need one
   foo(A, a(A)):- b(A).
will compile to h_void, h_functor(a/1), h_var(0), h_pop, i_enter, b_var(0), i_depart(b/1).
Wherease
   foo(a(A), A):- b(A).
will compile to h_functor(a/1), h_firstvar(2), h_pop, h_var(2), i_enter, b_var(2), i_depart(b/1).

Remember that slots 0 and 1 correspond to the 2 arguments of foo/2. In the second case we need one additional variable slot as well, slot 2.
*/

function compileClause(term, instructions)
{
    var argSlots;
    if (term instanceof CompoundTerm && term.functor.equals(Constants.clauseFunctor))
    {
        // A clause
        if (term.args[0] instanceof CompoundTerm)
            argSlots = term.args[0].functor.arity;
        else
            argSlots = 0;
        // Now we must analyze the variables. Each variable will be given the following attributes:
        // fresh: true (this will be later altered by the compiler)
        // slot:  0..N (the location of the variable)
        var variables = {};
        var localCutSlots = getReservedEnvironmentSlots(term.args[1])
        context = {nextSlot:argSlots + localCutSlots};
        analyzeVariables(term.args[0], true, 0, variables, context);
        analyzeVariables(term.args[1], false, 1, variables, context);
        compileHead(term.args[0], variables, instructions);
        instructions.push({opcode: Instructions.iEnter});
        if (!compileBody(term.args[1], variables, instructions, true, {nextReserved:0}, -1))
            Errors.typeError(Constants.callableAtom, term.args[1]);
    }
    else
    {
        // Fact. A fact obviously cannot have any cuts in it, so there is nothing to reserve space for
        if (term instanceof CompoundTerm)
            argSlots = term.functor.arity;
        else
            argSlots = 0;
        context = {nextSlot:argSlots};
        var variables = {};
        analyzeVariables(term, true, 0, variables, context);
        compileHead(term, variables, instructions);
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
        if (arg.name == "_")
        {
            instructions.push({opcode: Instructions.hVoid})
        }
        else if (variables[arg.name].fresh)
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
        instructions.push({opcode: Instructions.hAtom,
			   atom: arg});
    }
    else if (arg instanceof IntegerTerm)
    {
        instructions.push({opcode: Instructions.hAtom,
                           atom: arg});
    }
    else if (arg instanceof FloatTerm)
    {
        instructions.push({opcode: Instructions.hAtom,
                           atom: arg});
    }
    else if (arg instanceof BigIntegerTerm)
    {
        instructions.push({opcode: Instructions.hAtom,
                           atom: arg});
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

function compileBody(term, variables, instructions, isTailGoal, reservedContext, localCutPoint)
{
    var rc = true;
    if (term instanceof VariableTerm)
    {
        compileTermCreation(term, variables, instructions);
        instructions.push({opcode: Instructions.iUserCall})
        if (isTailGoal)
            instructions.push({opcode: Instructions.iExit});
    }
    else if (term.equals(Constants.cutAtom))
    {
        if (localCutPoint != -1)
            instructions.push({opcode: Instructions.cLCut,
                               slot: localCutPoint});
        else
            instructions.push({opcode: Instructions.iCut});
        if (isTailGoal)
            instructions.push({opcode: Instructions.iExit});
    }
    else if (term.equals(Constants.trueAtom))
    {
        if (isTailGoal)
            instructions.push({opcode: Instructions.iExit});
    }
    else if (term.equals(Constants.catchAtom))
    {
        var slot = reservedContext.nextReserved++;
        instructions.push({opcode: Instructions.iCatch,
                           slot: slot});
        instructions.push({opcode: Instructions.iExitCatch,
                           slot: slot});
    }
    else if (term.equals(Constants.failAtom))
    {
	instructions.push({opcode: Instructions.iFail});
    }
    else if (term instanceof AtomTerm)
    {
	instructions.push({opcode: isTailGoal?Instructions.iDepart:Instructions.iCall,
			   functor: new Functor(term, 0)});
    }
    else if (term instanceof CompoundTerm)
    {
	if (term.functor.equals(Constants.conjunctionFunctor))
	{
            rc &= compileBody(term.args[0], variables, instructions, false, reservedContext, localCutPoint) &&
                compileBody(term.args[1], variables, instructions, isTailGoal, reservedContext, localCutPoint);
	}
	else if (term.functor.equals(Constants.disjunctionFunctor))
        {
            if (term.args[0] instanceof CompoundTerm && term.args[0].functor.equals(Constants.localCutFunctor))
            {
                // If-then-else
                var ifThenElseInstruction = instructions.length;
                var cutPoint = reservedContext.nextReserved++;
                instructions.push({opcode: Instructions.cIfThenElse,
                                   slot:cutPoint,
                                   address: -1});
                // If
                rc &= compileBody(term.args[0].args[0], variables, instructions, false, reservedContext, cutPoint);
                // (Cut)
                instructions.push({opcode: Instructions.cCut,
                                   slot: cutPoint});
                // Then
                rc &= compileBody(term.args[0].args[1], variables, instructions, false, reservedContext, localCutPoint);
                // (and now jump out before the Else)
                var jumpInstruction = instructions.length;
                instructions.push({opcode: Instructions.cJump,
                                   address: -1});
                // Else - we resume from here if the 'If' doesnt work out
                instructions[ifThenElseInstruction].address = instructions.length;
                rc &= compileBody(term.args[1], variables, instructions, false, reservedContext, localCutPoint);
                instructions[jumpInstruction].address = instructions.length;
            }
            else
            {
                // Ordinary disjunction
                var orInstruction = instructions.length;
                instructions.push({opcode: Instructions.cOr,
                                   address: -1});
                rc &= compileBody(term.args[0], variables, instructions, false, reservedContext, localCutPoint);
                var jumpInstruction = instructions.length;
                instructions.push({opcode: Instructions.cJump,
                                   address: -1});
                instructions[orInstruction].address = instructions.length;
                rc &= compileBody(term.args[1], variables, instructions, isTailGoal, reservedContext, localCutPoint);
                instructions[jumpInstruction].address = instructions.length;
            }
            // Finally, we need to make sure that the c_jump has somewhere to go,
            // so if this was otherwise the tail goal, throw in an i_exit
            if (isTailGoal)
                instructions.push({opcode: Instructions.iExit});
        }
        else if (term.functor.equals(Constants.notFunctor))
        {
            // \+A is the same as (A -> fail ; true)
            // It is compiled to a much simpler representation though:
            //    c_if_then_else N
            //    <code for A>
            //    c_cut
            //    i_fail
            // N: (rest of the clause)
            var ifThenElseInstruction = instructions.length;
            var cutPoint = reservedContext.nextReserved++;
            instructions.push({opcode: Instructions.cIfThenElse,
                               slot:cutPoint,
                               address: -1});
            // If
            if (!compileBody(term.args[0], variables, instructions, false, reservedContext, cutPoint))
                Errors.typeError(Constants.callableAtom, term.args[0]);

            // (Cut)
            instructions.push({opcode: Instructions.cCut,
                               slot: cutPoint});
            // Then
            instructions.push({opcode: Instructions.iFail});
            // Else - we resume from here if the 'If' doesnt work out
            instructions[ifThenElseInstruction].address = instructions.length;
            if (isTailGoal)
                instructions.push({opcode: Instructions.iExit});
        }
	else if (term.functor.equals(Constants.throwFunctor))
        {
	    compileTermCreation(term.args[0], variables, instructions);
            instructions.push({opcode: Instructions.bThrow});
	}
        else if (term.functor.equals(Constants.crossModuleCallFunctor))
        {
            instructions.push({opcode: Instructions.iSwitchModule,
                               atom: term.args[0]});
            rc &= compileBody(term.args[1], variables, instructions, false, reservedContext, localCutPoint);
            instructions.push({opcode: Instructions.iExitModule});
            if (isTailGoal)
                instructions.push({opcode: Instructions.iExit});

        }
        else if (term.functor.equals(Constants.cleanupChoicepointFunctor))
        {
            compileTermCreation(term.args[0], variables, instructions);
            compileTermCreation(term.args[1], variables, instructions);
            instructions.push({opcode: Instructions.bCleanupChoicepoint});
	}

        else if (term.functor.equals(Constants.localCutFunctor))
        {
            var cutPoint = reservedContext.nextReserved++;
            instructions.push({opcode: Instructions.cIfThen,
                               slot:cutPoint});
            rc &= compileBody(term.args[0], variables, instructions, false, reservedContext, cutPoint);
            instructions.push({opcode: Instructions.cCut,
                               slot: cutPoint});
            rc &= compileBody(term.args[1], variables, instructions, false, reservedContext, localCutPoint);
            if (isTailGoal)
                instructions.push({opcode: Instructions.iExit});
        }
        else if (term.functor.equals(Constants.notUnifiableFunctor))
        {
            // This is just \+(A=B)
            rc &= compileBody(new CompoundTerm(Constants.notFunctor, [new CompoundTerm(Constants.unifyFunctor, [term.args[0], term.args[1]])]), variables, instructions, isTailGoal, reservedContext, localCutPoint);
        }
	else if (term.functor.equals(Constants.unifyFunctor))
        {
	    compileTermCreation(term.args[0], variables, instructions);
	    compileTermCreation(term.args[1], variables, instructions);
	    instructions.push({opcode: Instructions.iUnify});
	    if (isTailGoal)
		instructions.push({opcode: Instructions.iExit});
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
        rc = false;
        //Errors.typeError(Constants.callableAtom, term);
    }
    return rc;
}

function compileTermCreation(term, variables, instructions)
{
    if (term instanceof VariableTerm)
    {
        if (term.name == "_")
        {
            instructions.push({opcode: Instructions.bVoid});
        }
        else if (variables[term.name].fresh)
	{
	    instructions.push({opcode: Instructions.bFirstVar,
			       name:term.name,
			       slot:variables[term.name].slot});
	    variables[term.name].fresh = false;
	}
        else if (false) // FIXME: Do we need bArgVar?
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
    else if (term instanceof AtomTerm)
    {
	instructions.push({opcode: Instructions.bAtom,
			   atom:term});
    }
    else if (term instanceof IntegerTerm)
    {
        instructions.push({opcode: Instructions.bAtom,
                           atom:term});

    }
    else if (term instanceof FloatTerm)
    {
        instructions.push({opcode: Instructions.bAtom,
                           atom:term});

    }
    else if (term instanceof BigIntegerTerm)
    {
        instructions.push({opcode: Instructions.bAtom,
                           atom:term});
    }
    else if (term instanceof BlobTerm)
    {
        // CHECKME: This is a bit suspicious, but what do we do about open(foo, read, S), assert(bar(S)) ?
        instructions.push({opcode: Instructions.bAtom,
                           atom:term});
    }
    else
    {
        throw "Bad type: " + term.getClass();
    }
}


// Here we reserve extra slots in the environment for two purposes:
//   1) Cuts - each local cut appearing in the body must have its own slot so we can save the value of B0
//      this is because when executing (foo -> bar) the act of calling foo/0 can change B0 (indeed, it will set it to B on entry)
//   2) Exceptions - each catch/3 term needs two slots in the environment: One for the associated cut, and one for the ball
// We have to pull apart any goals that we compile away and add slots for their arguments as well since they will be flattened into the
// body itself. This includes ,/2, ;/2, ->/2 and catch/3
function getReservedEnvironmentSlots(term)
{
    var slots = 0;
    if (term instanceof CompoundTerm)
    {
	if (term.functor.equals(Constants.conjunctionFunctor))
	{
            slots += getReservedEnvironmentSlots(term.args[0], context);
	    slots += getReservedEnvironmentSlots(term.args[1], context);
	}
	else if (term.functor.equals(Constants.disjunctionFunctor))
        {
	    slots += getReservedEnvironmentSlots(term.args[0], context);
	    slots += getReservedEnvironmentSlots(term.args[1], context);
	}
	else if (term.functor.equals(Constants.localCutFunctor))
	{
            slots++;
	    slots += getReservedEnvironmentSlots(term.args[0], context);
	    slots += getReservedEnvironmentSlots(term.args[1], context);
        }
    }
    return slots;
}

// analyzeVariables builds a map of all the variables and returns the number of (unique, nonvoid) variables encountered
function analyzeVariables(term, isHead, depth, map, context)
{
    rc = 0;
    if (term instanceof VariableTerm)
    {
        if (term.name == "_") // _ is always void
            return rc;
	if (map[term.name] === undefined)
	{
	    map[term.name] = ({variable: term,
                               fresh: true,
                               slot: context.nextSlot++});
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
						   fresh: true,
                                                   slot: i});
			rc++;
		    }
                }
                else
                    rc += analyzeVariables(term.args[i], isHead, depth+1, map, context);
            }
        }
        else
        {
            for (var i = 0; i < term.functor.arity; i++)
                rc += analyzeVariables(term.args[i], isHead, depth+1, map, context);
        }
    }
    return rc;
}

function foreignShim(fn)
{
    var instructions = [];
    instructions.push({opcode: Instructions.iForeign,
                       foreign_function: fn});
    instructions.push({opcode: Instructions.iForeignRetry});
    return assemble(instructions);
}

function failShim()
{
    var instructions = [];
    instructions.push({opcode: Instructions.iFail});
    var compiled = assemble(instructions);
    return {opcodes: compiled.bytecode,
            constants: compiled.constants};
}

module.exports = {compilePredicate:compilePredicate,
                  compileQuery:compileQuery,
                  findVariables:findVariables,
                  foreignShim: foreignShim,
                  fail: failShim};
