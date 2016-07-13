function Clause(bytecode, constants, instructions)
{
    this.opcodes = bytecode;
    this.constants = constants;
    this.instructions = instructions;
    this.nextClause = null;
}

module.exports = Clause;
