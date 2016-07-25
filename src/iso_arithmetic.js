"use strict";
exports=module.exports;

var Arithmetic = require('./arithmetic');
var CTable = require('./ctable');

// 8.6.1
module.exports.is = function(result, expr)
{
    var q = Arithmetic.evaluate(expr);
    q = (CTable.intern(q) | 0xc0000000) & 0xffffffff;
    return this.unify(result, q);
}

// 8.7.1 Arithmetic comparison
module.exports["=\\="] = function(a, b)
{
    return Arithmetic.compare(a, b) != 0;
}
module.exports["=:="] = function(a, b)
{
    return Arithmetic.compare(a, b) == 0;
}
module.exports[">"] = function(a, b)
{
    return Arithmetic.compare(a, b) > 0;
}
module.exports[">="] = function(a, b)
{
    return Arithmetic.compare(a, b) >= 0;
}
module.exports["=<"] = function(a, b)
{
    return Arithmetic.compare(a, b) <= 0;
}
module.exports["<"] = function(a, b)
{
    return Arithmetic.compare(a, b) < 0;
}
