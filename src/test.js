var env = new Prolog.Environment();
var arg = new Prolog.VariableTerm("Input");
env.consultURL("http://localhost:8080/bin/x.pl", function()
               {
                   var query = new Prolog.CompoundTerm("f", [arg]);
                   env.execute(query,
                               onSuccess,
                               onFail,
                               onError);
               });

function onSuccess(withChoicepoints)
{
    console.log(">>>>>>>> Result: " + arg.dereference());
    if (withChoicepoints)
    {
        console.log("Trying to backtrack...");
        env.backtrack(onSuccess, onFail, onError);
    }
    else
        console.log("No further solutions detected");
}

function onFail()
{
    console.log("Failed");
}

function onError(error)
{
    console.log("Error: " + PORTRAY(error));
}
