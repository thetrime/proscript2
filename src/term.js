function Term()
{
}

Term.prototype.dereference = function()
{
    return this;
}

Term.prototype.dereference_recursive = function()
{
    return this;
}


module.exports = Term;
