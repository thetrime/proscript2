var LOOKUP_OPCODE = require('./opcodes.js').opcodes;

function execute(bytecode)
{
    var ptr = 0;
    while(true)
    {
	if (bytecode[ptr] === undefined)
	    throw("Illegal fetch");
	console.log(LOOKUP_OPCODE[bytecode[ptr]].label);
	switch(LOOKUP_OPCODE[bytecode[ptr]].label)
	{
	    case "i_fail":
	    ptr++;
	    continue;
	    case "i_save_cut":
	    ptr++;
	    continue;
	    case "i_enter":
	    ptr++;
	    continue;
	    case "i_exit":
	    ptr++;
	    continue;
	    case "i_exitfact":
	    ptr++;
	    continue;
	    case "i_depart":
	    ptr+=3;
	    continue;
	    case "i_call":
	    ptr+=3;
	    continue;
	    case "i_cut":
	    ptr++;
	    continue;
	    case "i_unify":
	    ptr++;
	    continue;
	    case "b_firstvar":
	    ptr+=3;
	    continue;
	    case "b_argvar":
	    ptr+=3;
	    continue;
	    case "b_var":
	    ptr+=3;
	    continue;
	    case "b_pop":
	    ptr++;
	    continue;
	    case "b_atom":
	    ptr+=3;
	    continue;
	    case "b_functor":
	    ptr+=3;
	    continue;
	    case "h_firstvar":
	    ptr+=3;
	    continue;
	    case "h_pop":
	    ptr++;
	    continue;
	    case "h_void":
	    ptr++;
	    continue;
	    case "h_var":
	    ptr+=3;
	    continue;
	    case "try_me_else":
	    ptr+=5;
	    continue;
	    case "retry_me_else":
	    ptr+=5;
	    continue;
	    case "trust_me":
	    ptr+=5;
	    continue;
	}
    }
}

module.exports = {execute: execute};
