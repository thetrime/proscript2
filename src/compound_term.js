var Functor = require('./functor.js');

function CompoundTerm(functor_name, args)
{
    this.functor = Functor.get(functor_name, args.length);
    this.args = args;
}

CompoundTerm.prototype.dereference = function()
{
    return this;
}


module.exports = CompoundTerm;
