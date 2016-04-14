var atom_table = {};

function AtomTerm(value)
{
    this.value = value;
}

AtomTerm.prototype.dereference = function()
{
    return this;
}

AtomTerm.get = function(value)
{
    if (atom_table[value] === undefined)
	atom_table[value] = new AtomTerm(value);
    return atom_table[value];
}


module.exports = AtomTerm;
