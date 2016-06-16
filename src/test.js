var Compiler = require('./compiler.js');
var Parser = require('./parser.js');
var Environment = require('./environment.js');
var util = require('util');
var CompoundTerm = require('./compound_term.js');
var VariableTerm = require('./variable_term.js');
var AtomTerm = require('./atom_term.js');
var Functor = require('./functor.js');
var Util = require('util');


debug_msg = function (msg)
{
    console.log(msg);
}

debug = function (msg)
{
    console.log(msg);
}



var env = new Environment();
env.consultString("=(X,X).");
env.consultString("foo(X):- bar(X), qux(X).");
env.consultString("bar(a).");
env.consultString("bar(b).");
env.consultString("bar(c(C)):- C = cat.");

env.consultString("qux(c(X)):- baz(X)");
env.consultString("baz(mouse).");
env.consultString("baz(cat).");

//env.getPredicateCode(Functor.get(AtomTerm.get("foo"), 1));
//env.getPredicateCode(Functor.get(AtomTerm.get("qux"), 1));
//throw "halt";

var arg = new VariableTerm("A");
var query = new CompoundTerm("foo", [arg]);
if (!env.execute(query))
    console.log("Failed");
else
{
    console.log("Result: " + util.inspect(arg, {showHidden: false, depth: null}));
    console.log("Or more simply: " + arg.dereference());
}

