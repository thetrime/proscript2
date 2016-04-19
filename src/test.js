var Compiler = require('./compiler.js');
var Parser = require('./parser.js');
var Environment = require('./environment.js');
var util = require('util');
var CompoundTerm = require('./compound_term.js');
var VariableTerm = require('./variable_term.js');
var AtomTerm = require('./atom_term.js');


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
env.consultString("splunge(X, _, X).");
env.consultString("writeln(_).");


var query = new CompoundTerm("foo", [AtomTerm.get("a"), new VariableTerm("B"), new VariableTerm("C"), new VariableTerm("D")]);

env.execute(query);

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
