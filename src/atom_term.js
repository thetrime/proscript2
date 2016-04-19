var atom_map = {};
var atom_index = 0;
var atom_table = [];

function AtomTerm(value)
{
    this.value = value;
    atom_table[atom_index] = this;
    this.index = atom_index;
    atom_index++;
}

AtomTerm.prototype.dereference = function()
{
    return this;
}

AtomTerm.get = function(value)
{
    if (atom_map[value] === undefined)
	atom_map[value] = new AtomTerm(value);
    return atom_map[value];
}

AtomTerm.lookup = function(i)
{
    return atom_table[i];
}

module.exports = AtomTerm;
