"use strict";
exports=module.exports;

var Term = require('./term');

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

FloatTerm.prototype.getClass = function()
{
    return "FloatTerm";
}


module.exports = FloatTerm;

