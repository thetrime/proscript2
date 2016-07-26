"use strict";
exports=module.exports;

var Constants = require('./constants.js');
var CompoundTerm = require('./compound_term.js');
var AtomTerm = require('./atom_term.js');
var BlobTerm = require('./blob_term.js');
var FloatTerm = require('./float_term.js');
var BigIntegerTerm = require('./biginteger_term.js');
var IntegerTerm = require('./integer_term.js');
var Functor = require('./functor.js');
var Errors = require('./errors.js');
var Clause = require('./clause.js');
var CTable = require('./ctable.js');
var Instructions = require('./opcodes.js').opcode_map;
var util = require('util');


function compilePredicateClause(clause, withChoicepoint, meta)
{
    var instructions = [];
    if (meta !== false)
    {
        for (var i = 0; i < meta.length; i++)
        {
            if (meta[i] == "+" || meta[i] == "?" || meta[i] == "-")
                continue;
            instructions.push({opcode: Instructions.s_qualify,
                               slot: [i]});
        }
    }
    if (withChoicepoint)
        instructions.push({opcode: Instructions.try_me_or_next_clause});
    compileClause(clause, instructions);
    return assemble(instructions);
}

function compilePredicate(clauses, meta)
{
    if (clauses == [])
    {
        return assemble([{opcode: Instructions.i_fail}]);
    }
    var lastClause = null;
    var firstClause = null;
    for (var i = 0; i < clauses.length; i++)
    {
        var assembled = compilePredicateClause(clauses[i], i+1 < clauses.length, i==0?meta:false);
        if (lastClause != null)
            lastClause.nextClause = assembled;
        else
            firstClause = assembled;
        lastClause = assembled;
    }
    return firstClause;
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
    return new Clause(bytes, constants, instructions);
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
            var index = constants.indexOf(instruction.functor);
	    if (index == -1)
		index = constants.push(instruction.functor) - 1;
	    bytecode[ptr+1] = (index >> 8) & 255
            bytecode[ptr+2] = (index >> 0) & 255
            rc += 2;
            ptr += 2;
	}
	else if (arg == "atom")
	{
	    var index = constants.indexOf(instruction.atom);
	    if (index == -1)
		index = constants.push(instruction.atom) - 1;
	    bytecode[ptr+1] = (index >> 8) & 255
	    bytecode[ptr+2] = (index >> 0) & 255
            rc += 2;
            ptr += 2;
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
            ptr += 4;
	}
	else if (arg == "slot")
        {
            bytecode[ptr+1] = (instruction.slot >> 8) & 255;
            bytecode[ptr+2] = (instruction.slot >> 0) & 255;
            rc += 2;
            ptr += 2;
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
    compileClause(CompoundTerm.create(Constants.clauseFunctor, [CompoundTerm.create("$query", variables), term]), instructions);
    //console.log(util.inspect(code, {showHidden: false, depth: null}));
    return {clause: assemble(instructions),
            variables: variables};
}

function findVariables(term, variables)
{
    term = DEREF(term);
    if (TAGOF(term) == VariableTag && variables.indexOf(term) == -1)
        variables.push(term);
    else if (TAGOF(term) == CompoundTag)
    {
        var ftor = FUNCTOROF(term);
        var ftor_object = CTable.get(ftor);
        for (var i = 0; i < ftor_object.arity; i++)
            findVariables(ARGOF(term, i), variables);
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
    if (TAGOF(term) == CompoundTag && FUNCTOROF(term) == Constants.clauseFunctor)
    {
        // A clause
        if (TAGOF(ARGOF(term, 0)) == CompoundTag)
            argSlots = CTable.get(FUNCTOROF(ARGOF(term,0))).arity;
        else
            argSlots = 0;
        // Now we must analyze the variables. Each variable will be given the following attributes:
        // fresh: true (this will be later altered by the compiler)
        // slot:  0..N (the location of the variable)
        var variables = {};
        var localCutSlots = getReservedEnvironmentSlots(ARGOF(term, 1));
        var context = {nextSlot:argSlots + localCutSlots};
        analyzeVariables(ARGOF(term, 0), true, 0, variables, context);
        analyzeVariables(ARGOF(term, 1), false, 1, variables, context);
        compileHead(ARGOF(term, 0), variables, instructions);
        instructions.push({opcode: Instructions.i_enter});
        if (!compileBody(ARGOF(term, 1), variables, instructions, true, {nextReserved:0}, -1))
        {
            var badBody = ARGOF(term, 1);
            if (TAGOF(badBody) == CompoundTag && FUNCTOROF(badBody) == Constants.crossModuleCallFunctor)
                Errors.typeError(Constants.callableAtom, ARGOF(ARGOF(term, 1), 1));
            else
                Errors.typeError(Constants.callableAtom, ARGOF(term, 1));
        }
    }
    else
    {
        // Fact. A fact obviously cannot have any cuts in it, so there is nothing to reserve space for
        if (TAGOF(term) == CompoundTag)
            argSlots = CTable.get(FUNCTOROF(term)).arity;
        else
            argSlots = 0;
        var context = {nextSlot:argSlots};
        var variables = {};
        analyzeVariables(term, true, 0, variables, context);
        compileHead(term, variables, instructions);
        instructions.push({opcode: Instructions.i_exitfact});
    }
}


function compileHead(term, variables, instructions)
{
    var qqq = instructions.length;
    var optimizeRef = instructions.length;
    if (TAGOF(term) == CompoundTag)
    {
        var functor = CTable.get(FUNCTOROF(term));
        for (var i = 0; i < functor.arity; i++)
	{
            if (!compileArgument(ARGOF(term, i), variables, instructions, false))
                optimizeRef = instructions.length;
	}
    }
    if (optimizeRef < instructions.length && false)
    {
        // This is a very simple optimization: If we ONLY have h_void instructions before i_enter then we can entirely omit them
        // FIXME: For some reason this causes things to go horribly wrong. See the test suite
        instructions.splice(optimizeRef, instructions.length);
    }
}

function compileArgument(arg, variables, instructions, embeddedInTerm)
{
    if (TAGOF(arg) == VariableTag)
    {
        if (arg == 0) // Anonymous variable _
        {
            instructions.push({opcode: Instructions.h_void})
            return true;
        }
        else if (variables[arg].fresh)
        {
            variables[arg].fresh = false;
	    if (embeddedInTerm)
            {
                instructions.push({opcode: Instructions.h_firstvar,
                                   slot:variables[arg].slot});
                return false;
	    }
	    else
	    {
                instructions.push({opcode: Instructions.h_void});
                return true;
	    }
	}
        else
        {
            instructions.push({opcode: Instructions.h_var,
                               slot: variables[arg].slot});
            return false;
        }
    }
    else if (TAGOF(arg) == ConstantTag)
    {
        instructions.push({opcode: Instructions.h_atom,
                           atom: arg});
        return false;
    }
    else if (TAGOF(arg) == CompoundTag)
    {
        instructions.push({opcode: Instructions.h_functor,
                           functor: FUNCTOROF(arg)});
        var functor = CTable.get(FUNCTOROF(arg));
        for (var i = 0; i < functor.arity; i++)
            compileArgument(ARGOF(arg, i), variables, instructions, true);
        instructions.push({opcode: Instructions.h_pop});
        return false;
    }
}

function compileBody(term, variables, instructions, isTailGoal, reservedContext, localCutPoint)
{
    var rc = true;
    if (term != (term & 0xffffffff))
    {
        throw new Error();
    }
    if (TAGOF(term) == VariableTag)
    {
        compileTermCreation(term, variables, instructions);
        instructions.push({opcode: Instructions.i_usercall})
        if (isTailGoal)
            instructions.push({opcode: Instructions.i_exit});
    }
    else if (term == Constants.cutAtom)
    {
        if (localCutPoint != -1)
            instructions.push({opcode: Instructions.c_lcut,
                               slot: localCutPoint});
        else
            instructions.push({opcode: Instructions.i_cut});
        if (isTailGoal)
            instructions.push({opcode: Instructions.i_exit});
    }
    else if (term == Constants.trueAtom)
    {
        if (isTailGoal)
            instructions.push({opcode: Instructions.i_exit});
    }
    else if (term == Constants.catchAtom)
    {
        var slot = reservedContext.nextReserved++;
        instructions.push({opcode: Instructions.i_catch,
                           slot: slot});
        instructions.push({opcode: Instructions.i_exitcatch,
                           slot: slot});
    }
    else if (term == Constants.failAtom)
    {
        instructions.push({opcode: Instructions.i_fail});
    }
    else if (TAGOF(term) == ConstantTag && CTable.get(term) instanceof AtomTerm)
    {
        if (CTable.get(term).value == "fail")
        {
            console.log(term & 0xffffffff);
            console.log("Fail constant: " + Constants.failAtom);
            console.log("Test: " + AtomTerm.get("fail"));
            throw new Error("NO");
        }
        instructions.push({opcode: isTailGoal?Instructions.i_depart:Instructions.i_call,
                           functor: Functor.get(term, 0)});
    }
    else if (TAGOF(term) == CompoundTag)
    {
        if (FUNCTOROF(term) == Constants.conjunctionFunctor)
	{
            rc &= compileBody(ARGOF(term, 0), variables, instructions, false, reservedContext, localCutPoint) &&
                compileBody(ARGOF(term, 1), variables, instructions, isTailGoal, reservedContext, localCutPoint);
	}
        else if (FUNCTOROF(term) == Constants.disjunctionFunctor)
        {
            if (TAGOF(ARGOF(term, 0)) == CompoundTag && FUNCTOROF(ARGOF(term, 0)) == Constants.localCutFunctor)
            {
                // If-then-else
                var ifThenElseInstruction = instructions.length;
                var cutPoint = reservedContext.nextReserved++;
                instructions.push({opcode: Instructions.c_ifthenelse,
                                   slot:cutPoint,
                                   address: -1});
                // If
                rc &= compileBody(ARGOF(ARGOF(term, 0), 0), variables, instructions, false, reservedContext, cutPoint);
                // (Cut)
                instructions.push({opcode: Instructions.c_cut,
                                   slot: cutPoint});
                // Then
                rc &= compileBody(ARGOF(ARGOF(term, 0), 1), variables, instructions, false, reservedContext, localCutPoint);
                // (and now jump out before the Else)
                var jumpInstruction = instructions.length;
                instructions.push({opcode: Instructions.c_jump,
                                   address: -1});
                // Else - we resume from here if the 'If' doesnt work out
                instructions[ifThenElseInstruction].address = instructions.length;
                rc &= compileBody(ARGOF(term, 1), variables, instructions, false, reservedContext, localCutPoint);
                instructions[jumpInstruction].address = instructions.length;
            }
            else
            {
                // Ordinary disjunction
                var orInstruction = instructions.length;
                instructions.push({opcode: Instructions.c_or,
                                   address: -1});
                rc &= compileBody(ARGOF(term, 0), variables, instructions, false, reservedContext, localCutPoint);
                var jumpInstruction = instructions.length;
                instructions.push({opcode: Instructions.c_jump,
                                   address: -1});
                instructions[orInstruction].address = instructions.length;
                rc &= compileBody(ARGOF(term, 1), variables, instructions, isTailGoal, reservedContext, localCutPoint);
                instructions[jumpInstruction].address = instructions.length;
            }
            // Finally, we need to make sure that the c_jump has somewhere to go,
            // so if this was otherwise the tail goal, throw in an i_exit
            if (isTailGoal)
                instructions.push({opcode: Instructions.i_exit});
        }
        else if (FUNCTOROF(term) == Constants.notFunctor)
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
            instructions.push({opcode: Instructions.c_ifthenelse,
                               slot:cutPoint,
                               address: -1});
            // If
            if (!compileBody(ARGOF(term, 0), variables, instructions, false, reservedContext, cutPoint))
                Errors.typeError(Constants.callableAtom, ARGOF(term, 0));

            // (Cut)
            instructions.push({opcode: Instructions.c_cut,
                               slot: cutPoint});
            // Then
            instructions.push({opcode: Instructions.i_fail});
            // Else - we resume from here if the 'If' doesnt work out
            instructions[ifThenElseInstruction].address = instructions.length;
            if (isTailGoal)
                instructions.push({opcode: Instructions.i_exit});
        }
        else if (FUNCTOROF(term) == Constants.throwFunctor)
        {
            compileTermCreation(ARGOF(term, 0), variables, instructions);
            instructions.push({opcode: Instructions.b_throw});
	}
        else if (FUNCTOROF(term) == Constants.crossModuleCallFunctor)
        {
            instructions.push({opcode: Instructions.i_switch_module,
                               atom: ARGOF(term, 0)});
            rc &= compileBody(ARGOF(term, 1), variables, instructions, false, reservedContext, localCutPoint);
            instructions.push({opcode: Instructions.i_exitmodule});
            if (isTailGoal)
                instructions.push({opcode: Instructions.i_exit});

        }
        else if (FUNCTOROF(term) == Constants.cleanupChoicepointFunctor)
        {
            compileTermCreation(ARGOF(term, 0), variables, instructions);
            compileTermCreation(ARGOF(term, 1), variables, instructions);
            instructions.push({opcode: Instructions.b_cleanup_choicepoint});
	}

        else if (FUNCTOROF(term) == Constants.localCutFunctor)
        {
            var cutPoint = reservedContext.nextReserved++;
            instructions.push({opcode: Instructions.c_ifthen,
                               slot:cutPoint});
            rc &= compileBody(ARGOF(term, 0), variables, instructions, false, reservedContext, cutPoint);
            instructions.push({opcode: Instructions.c_cut,
                               slot: cutPoint});
            rc &= compileBody(ARGOF(term, 1), variables, instructions, false, reservedContext, localCutPoint);
            if (isTailGoal)
                instructions.push({opcode: Instructions.i_exit});
        }
        else if (FUNCTOROF(term) == Constants.notUnifiableFunctor)
        {
            // This is just \+(A=B)
            rc &= compileBody(CompoundTerm.create(Constants.notFunctor, [CompoundTerm.create(Constants.unifyFunctor, [ARGOF(term, 0), ARGOF(term, 1)])]), variables, instructions, isTailGoal, reservedContext, localCutPoint);
        }
        else if (FUNCTOROF(term) == Constants.unifyFunctor)
        {
            compileTermCreation(ARGOF(term, 0), variables, instructions);
            compileTermCreation(ARGOF(term, 1), variables, instructions);
            instructions.push({opcode: Instructions.i_unify});
	    if (isTailGoal)
                instructions.push({opcode: Instructions.i_exit});
	}
	else
        {
            var functor = CTable.get(FUNCTOROF(term));
            for (var i = 0; i < functor.arity; i++)
                compileTermCreation(ARGOF(term, i), variables, instructions);
            instructions.push({opcode: isTailGoal?Instructions.i_depart:Instructions.i_call,
                               functor: FUNCTOROF(term)});
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
    //console.log("Compiling: " + term);
    if (term == 0) console.log("Compiling null term: " + new Error().stack);
    if (TAGOF(term) == VariableTag)
    {
        if (term == 0) // anonymous variable _
        {
            instructions.push({opcode: Instructions.b_void});
        }
        else if (variables[term].fresh)
	{
            instructions.push({opcode: Instructions.b_firstvar,
                               slot:variables[term].slot});
            variables[term].fresh = false;
        }
        /*
        else if (false) // FIXME: Do we need bArgVar?
	{
            instructions.push({opcode: Instructions.b_argvar,
			       name:term.name,
			       slot:variables[term.name].slot});
        }
        */
	else
	{
            instructions.push({opcode: Instructions.b_var,
                               name:term,
                               slot:variables[term].slot});

	}
    }
    else if (TAGOF(term) == CompoundTag)
    {
        instructions.push({opcode: Instructions.b_functor,
                           functor:FUNCTOROF(term)});
        var functor = CTable.get(FUNCTOROF(term));
        for (var i = 0; i < functor.arity; i++)
            compileTermCreation(ARGOF(term, i), variables, instructions);
        instructions.push({opcode: Instructions.b_pop});
    }
    else if (TAGOF(term) == ConstantTag)
    {
        instructions.push({opcode: Instructions.b_atom,
			   atom:term});
    }
    else
    {
        throw "Bad type: " + term;
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
    if (TAGOF(term) == CompoundTag)
    {
        if (FUNCTOROF(term) == Constants.conjunctionFunctor)
	{
            slots += getReservedEnvironmentSlots(ARGOF(term, 0));
            slots += getReservedEnvironmentSlots(ARGOF(term, 1));
	}
        else if (FUNCTOROF(term) == Constants.disjunctionFunctor)
        {
            slots += getReservedEnvironmentSlots(ARGOF(term, 0));
            slots += getReservedEnvironmentSlots(ARGOF(term, 1));
	}
        else if (FUNCTOROF(term) == Constants.localCutFunctor)
	{
            slots++;
            slots += getReservedEnvironmentSlots(ARGOF(term, 0));
            slots += getReservedEnvironmentSlots(ARGOF(term, 1));
        }
    }
    return slots;
}

// analyzeVariables builds a map of all the variables and returns the number of (unique, nonvoid) variables encountered
function analyzeVariables(term, isHead, depth, map, context)
{
    var rc = 0;
    if (TAGOF(term) == VariableTag)
    {
        if (term == 0) // _ is always void
            return rc;
        if (map[term] === undefined)
	{
            map[term] = ({variable: term,
                          fresh: true,
                          slot: context.nextSlot++});
	    rc++;
	}
    }
    else if (TAGOF(term) == CompoundTag)
    {
        var functor = CTable.get(FUNCTOROF(term));
	if (isHead && depth == 0)
	{
	    // Reserve the first arity slots since that is where the previous
            // frame is going to write the arguments
            for (var i = 0; i < functor.arity; i++)
            {
                if (TAGOF(ARGOF(term, i)) == VariableTag)
		{
                    if (map[ARGOF(term, i)] === undefined)
		    {
                        map[ARGOF(term, i)] = ({variable: ARGOF(term, i),
                                                fresh: true,
                                                slot: i});
			rc++;
		    }
                }
                else
                    rc += analyzeVariables(ARGOF(term, i), isHead, depth+1, map, context);
            }
        }
        else
        {
            for (var i = 0; i < functor.arity; i++)
                rc += analyzeVariables(ARGOF(term, i), isHead, depth+1, map, context);
        }
    }
    return rc;
}

function foreignShim(fn)
{
    var instructions = [];
    instructions.push({opcode: Instructions.i_foreign,
                       foreign_function: fn});
    instructions.push({opcode: Instructions.i_foreignretry});
    return assemble(instructions);
}

function failShim()
{
    return assemble([{opcode: Instructions.i_fail}]);
}

module.exports = {compilePredicate:compilePredicate,
                  compilePredicateClause:compilePredicateClause,
                  compileQuery:compileQuery,
                  findVariables:findVariables,
                  foreignShim: foreignShim,
                  fail: failShim};
