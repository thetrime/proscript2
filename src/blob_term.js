var AtomTerm = require('./atom_term');

var global_blob_id = 0;

function BlobTerm(type, value)
{
    this.value = value;
    this.id = global_blob_id++;
    this.type = type;
}

BlobTerm.prototype = new AtomTerm;

BlobTerm.prototype.toString = function()
{
    return "<" + this.type + ">(" + this.id +")";
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
