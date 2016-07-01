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

function toByteArray(str)
{
    var byteArray = [];
    for (var i = 0; i < str.length; i++)
    {
        if (str.charCodeAt(i) <= 0x7F)
        {
            byteArray.push(str.charCodeAt(i));
        }
        else 
        {
            var h = encodeURIComponent(str.charAt(i)).substr(1).split('%');
            for (var j = 0; j < h.length; j++)
            {
                byteArray.push(parseInt(h[j], 16));
            }
        }
    }
    return byteArray;
}


var env = new Environment();

function writeln(env, arg)
{
    var bytes = toByteArray(arg.toString());
    console.log(bytes);
    return env.streams.stdout.write(env.streams.stdout, 1, bytes.length, bytes) >= 0;
}

env.getModule().defineForeignPredicate("writeln", 1, writeln);
env.consultURL("http://localhost:8080/bin/x.pl", function()
	       {
                   var arg = new VariableTerm("Input");
                   var query = new CompoundTerm("error", [arg]);
                   if (!env.execute(query))
		       console.log("Failed");
		   else
		   {
		       console.log(">>>>>>>> Result: " + arg.dereference());
		       console.log("Trying to backtrack...");
		       while (env.backtrack())
		       {
			   console.log(">>>>>>>> Result: " + arg.dereference());
			   console.log("Trying to backtrack...");
		       }
		       console.log("No more solutions");
		   }
	       });
/*
env.consultString("foo(X):- bar(X), qux(X).");
env.consultString("bar(a).");
env.consultString("bar(b).");
env.consultString("bar(c(C)):- cat = C.");
env.consultString("bar(X):- X = c(cats).");
env.consultString("bar(c(d)):- x(q, y) = x(q, y).");
env.consultString("qux(c(X)):- baz(X).");
env.consultString("baz(mouse).");
env.consultString("baz(cats).");
env.consultString("baz(d).");
*/




