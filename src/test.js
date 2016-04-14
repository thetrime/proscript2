var compilePredicate = require('./compiler.js');
var Parser = require('./parser.js');

debug_msg = function (msg)
{
    console.log(msg);
}

debug = function (msg)
{
    console.log(msg);
}


var util = require('util');
var clause = Parser.test("foo(a, b):- writeln(c).");
console.log(util.inspect(clause, {showHidden: false, depth: null}));
var instructions = compilePredicate([clause]);
console.log(util.inspect(instructions, {showHidden: false, depth: null}));

