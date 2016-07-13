var Prolog = require('./core');
var env = new Prolog.Environment();
var test_file = "tests/t.pl";
env.consultFile(test_file, function()
                {
                    var d0 = Date.now();
                   if (!env.execute(new Prolog.AtomTerm("run_all_tests")))
                       console.log("Failed");
                   else
                       console.log("Succeeded");
                    var d1 = Date.now();
                    console.log("Execution (not compilation) time: " + (d1 - d0)/1000 + "s");
               });
