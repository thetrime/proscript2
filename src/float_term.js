"use strict";
exports=module.exports;

var Term = require('./term');
var CTable = require('./ctable');

function FloatTerm(value)
{
    this.value = value;
}

FloatTerm.prototype = new Term;

FloatTerm.prototype.equals = function(o)
{
    return (o === this) || (o instanceof FloatTerm && ((o || {}).value === this.value));
}

FloatTerm.prototype.toString = function()
{
    return this.value;
}

FloatTerm.prototype.hashCode = function()
{
    return this.value + "F";
}

FloatTerm.prototype.getClass = function()
{
    return "FloatTerm";
}

FloatTerm.get = function(i)
{
    return CTable.intern(new FloatTerm(i)) | 0xc0000000;
}


module.exports = FloatTerm;

