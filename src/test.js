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


var env = new Prolog.Environment();

function writeln(env, arg)
{
    var bytes = toByteArray(arg.toString());
    console.log(bytes);
    return env.streams.stdout.write(env.streams.stdout, 1, bytes.length, bytes) >= 0;
}

env.getModule().defineForeignPredicate("writeln", 1, writeln);
env.consultURL("http://localhost:8080/bin/x.pl", function()
	       {
                   var arg = new Prolog.VariableTerm("Input");
                   var query = new Prolog.CompoundTerm("error", [arg]);
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
