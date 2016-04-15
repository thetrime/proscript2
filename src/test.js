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
var clause = Parser.test("foo(A, B, C):- C is A + B, writeln(C).");
console.log(util.inspect(clause, {showHidden: false, depth: null}));
var instructions = compilePredicate([clause]);
console.log(util.inspect(instructions, {showHidden: false, depth: null}));

/*
push_functor is/2
firstVar C
push_functor +/2
firstVar A
firstVar B
var C
icall [idepart really] writeln/1
iexit
*/
