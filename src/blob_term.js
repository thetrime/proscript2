var AtomTerm = require('./atom_term');

var id = 0;

function BlobTerm(type, value)
{
    AtomTerm.call(this);
    this.value = value;
    this.id = id++;
    this.type = type;
}

BlobTerm.prototype.toString = function()
{
    return "<" + this.type + ">(" + id + ")";
}

BlobTerm.prototype.dereference = function()
{
    return this;
}

BlobTerm.prototype.getClass = function()
{
    return "blob";
}

module.exports = BlobTerm;
