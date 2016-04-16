var LOOKUP_OPCODE = require('./opcodes.js').opcodes;
var Functor = require('./functor.js');
var Frame = require('./frame.js');
var Module = require('./module.js');

function execute(currentFrame)
{
    var currentModule = Module.get("user");
    var PC = 0;
    var nextFrame = {slots: [],
                     code: undefined,
                     parent: currentFrame,
                     PC: undefined};
    while(true)
    {
	if (currentFrame.code[PC] === undefined)
	    throw("Illegal fetch");
	console.log(LOOKUP_OPCODE[currentFrame.code[PC]].label);
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
	    case "i_exit":
	    PC++;
	    continue;
	    case "i_exitfact":
	    PC++;
	    continue;
	    case "i_depart":
	    case "i_call":
            {
                var functor = Functor.lookup((currentFrame.code[PC+1] << 8) | (currentFrame.code[PC+2]));
                nextFrame.code = currentModule.getPredicate(functor);
                nextFrame.PC = PC+3;
                currentFrame = nextFrame;
	        PC = 0; // Start from the beginning of the frame
                continue;
            }
	    case "i_cut":
	    PC++;
	    continue;
	    case "i_unify":
	    PC++;
	    continue;
	    case "b_firstvar":
	    PC+=3;
	    continue;
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
	    case "h_pop":
	    PC++;
	    continue;
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
