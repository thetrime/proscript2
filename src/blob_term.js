"use strict";
exports=module.exports;

var AtomTerm = require('./atom_term');
var CTable = require('./ctable');

var global_blob_id = 0;

function BlobTerm(type, value)
{
    this.value = value;
    this.id = global_blob_id++;
    this.type = type;
}

BlobTerm.prototype = AtomTerm.get;

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

BlobTerm.prototype.hashCode = function()
{
    return "@" + this.type + "::" + this.id;
}

BlobTerm.get = function(type, data)
{
    return CTable.intern(new BlobTerm(type, data)) | 0xc0000000;
}

module.exports = BlobTerm;
