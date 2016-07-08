var env = new Prolog.Environment();
env.consultURL("http://localhost:8080/bin/x.pl", function()
	       {
                   var arg = new Prolog.VariableTerm("Input");
                   var query = new Prolog.CompoundTerm("f", [arg]);
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
