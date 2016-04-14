var functor_table = {};

function Functor(name, arity)
{
    this.name = name;
    this.arity = arity;
}

function get(name, arity)
{
    if (functor_table[name] === undefined)
	functor_table[name] = {};
    var arities = functor_table[name];
    if (arities[arity] === undefined)
	arities[arity] = new Functor(name, arity);
    return arities[arity];
}


module.exports = {get: get};
