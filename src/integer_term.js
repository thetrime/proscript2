"use strict";
exports=module.exports;

var Term = require('./term');
var CTable = require('./ctable');

function IntegerTerm(value)
{
    this.value = value;
}

IntegerTerm.prototype = new Term;

IntegerTerm.prototype.equals = function(o)
{
    return (o === this) || (o instanceof IntegerTerm && ((o || {}).value === this.value));
}

IntegerTerm.prototype.getClass = function()
{
    return "IntegerTerm";
}

IntegerTerm.prototype.toString = function()
{
    return this.value;
}

IntegerTerm.prototype.hashCode = function()
{
    return this.value;
}


IntegerTerm.get = function(i)
{
    return CTable.intern(new IntegerTerm(i)) | 0xc0000000;
}

module.exports = IntegerTerm;
