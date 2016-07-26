"use strict";
exports=module.exports;

var CTable = require('./ctable');

function AtomTerm(value)
{
    this.value = value;
}

AtomTerm.prototype.toString = function()
{
    return this.value;
}

AtomTerm.prototype.equals = function(o)
{
    return (o === this) || ((o || {}).value === this.value);
}

AtomTerm.prototype.getClass = function()
{
    return "AtomTerm";
}

AtomTerm.prototype.hashCode = function()
{
    return this.value;
}

AtomTerm.get = function(string)
{
    return (CTable.intern(new AtomTerm(string)) | 0xc0000000) & 0xffffffff;
}


module.exports = AtomTerm;
