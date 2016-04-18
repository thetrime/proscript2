var Compiler = require('./compiler.js');
var Parser = require('./parser.js');
var Environment = require('./environment.js');
var util = require('util');


debug_msg = function (msg)
{
    console.log(msg);
}

debug = function (msg)
{
    console.log(msg);
}



var env = new Environment();
env.consultString("foo(A, q(B), x, A):- splunge(X, A + B, x), !, writeln(X).");


var query = Parser.test("foo(a, B, C, D).");
console.log(util.inspect(query, {showHidden: false, depth: null}));
var queryCode = Compiler.compileQuery(query);
console.log(util.inspect(queryCode, {showHidden: false, depth: null}));

env.execute(queryCode);

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
