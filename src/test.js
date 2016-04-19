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
env.consultString("foo(a).");
var arg = new VariableTerm("A");
var query = new CompoundTerm("foo", [arg]);
env.execute(query);
console.log(arg);


/*
$query 0: i_enter
$query 1: b_firstvar
$query 4: i_depart
foo/1 0: h_atom
foo/1 3: i_enter
foo/1 4: i_exit
$top 0: i_exitquery
*/
