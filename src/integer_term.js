var integer_table = {};

function IntegerTerm(value)
{
    this.value = value;
}

IntegerTerm.prototype.dereference = function()
{
    return this;
}

IntegerTerm.get = function (value)
{
    if (integer_table[value] === undefined)
	integer_table[value] = new IntegerTerm(value);
    return integer_table[value];
}


module.exports = IntegerTerm;
