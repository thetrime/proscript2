var LOOKUP_OPCODE = require('./opcodes.js').opcodes;
var Functor = require('./functor.js');
var Frame = require('./frame.js');
var Module = require('./module.js');
var util = require('util');
var VariableTerm = require('./variable_term.js');
var AtomTerm = require('./atom_term.js');

function execute(env, currentFrame)
{
    var currentModule = Module.get("user");
    var PC = 0;
    var argP = 0;
    var nextFrame = new Frame(currentFrame);
    while(true)
    {
        if (currentFrame.code[PC] === undefined)
        {
            console.log(util.inspect(currentFrame));
            console.log("Illegal fetch at " + PC);
            throw("Illegal fetch");
        }
        console.log(currentFrame.functor + " " + PC + ": " + LOOKUP_OPCODE[currentFrame.code[PC]].label);
	switch(LOOKUP_OPCODE[currentFrame.code[PC]].label)
	{
	    case "i_fail":
	    PC++;
	    continue;
	    case "i_save_cut":
	    PC++;
	    continue;
	    case "i_enter":
	    PC++;
            continue;
            case "i_exitquery":
            {
                return;
            }
            case "i_exit":
            {
                PC = currentFrame.returnPC;
                currentFrame = currentFrame.parent;
                nextFrame = new Frame(currentFrame);
                continue;
            }
	    case "i_exitfact":
            {
                throw "not implemented";
            }
            case "i_depart":
            {
                var functor = Functor.lookup((currentFrame.code[PC+1] << 8) | (currentFrame.code[PC+2]));
                nextFrame.functor = functor;
                nextFrame.code = env.getPredicateCode(functor);
                nextFrame.returnPC = currentFrame.parent.returnPC;
                nextFrame.parent = currentFrame.parent;
                currentFrame = nextFrame;
                nextFrame = new Frame(currentFrame);
                // FIXME: Need to save argP - maybe not for i_depart
                argP = 0;
                PC = 0; // Start from the beginning of the code in the next frame
                continue;

            }
            case "i_call":
            {
                var functor = Functor.lookup((currentFrame.code[PC+1] << 8) | (currentFrame.code[PC+2]));
                nextFrame.functor = functor;
                nextFrame.code = env.getPredicateCode(functor);
                nextFrame.returnPC = PC+3;
                currentFrame = nextFrame;
                nextFrame = new Frame(currentFrame);
                // FIXME: Need to save argP I think
                argP = 0;
                PC = 0; // Start from the beginning of the code in the next frame
                continue;
            }
	    case "i_cut":
	    PC++;
	    continue;
	    case "i_unify":
	    PC++;
	    continue;
            case "b_firstvar":
            {
                var slot = ((currentFrame.code[PC+1] << 8) | (currentFrame.code[PC+2]));
                console.log("nextFrame.slots[" + argP + "] <- " + util.inspect(currentFrame.slots[slot]));
                nextFrame.slots[argP++] = currentFrame.slots[slot];
                PC+=3;
                continue;
            }
	    case "b_argvar":
	    PC+=3;
	    continue;
	    case "b_var":
	    PC+=3;
	    continue;
	    case "b_pop":
	    PC++;
	    continue;
	    case "b_atom":
	    PC+=3;
	    continue;
	    case "b_functor":
	    PC+=3;
	    continue;
	    case "h_firstvar":
	    PC+=3;
	    continue;
            case "h_functor":
	    PC+=3;
            continue;
            case "h_pop":
	    PC++;
	    continue;
            case "h_atom":
            {
                console.log(currentFrame.code);
                var atom = AtomTerm.lookup((currentFrame.code[PC+1] << 8) | (currentFrame.code[PC+2]));
                var arg = currentFrame.slots[argP]; // FIXME: Must deref
                PC+=3;
                console.log("Trying to match " + util.inspect(arg) + " and " + util.inspect(atom));
                if (arg == atom)
                    continue;
                if (arg instanceof VariableTerm)
                {
                    // FIXME: Must trail
                    arg.value = atom;
                }
                else
                {
                    // FIXME: Backtrack
                }
                continue;
            }
            case "h_void":
            {
	        PC++;
	        continue;
            }
	    case "h_var":
	    PC+=3;
	    continue;
	    case "try_me_else":
	    PC+=5;
	    continue;
	    case "retry_me_else":
	    PC+=5;
	    continue;
	    case "trust_me":
	    PC+=5;
	    continue;
	}
    }
}

module.exports = {execute: execute};
