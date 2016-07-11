var Prolog = require('./core');
var env = new Prolog.Environment();
env.consultFile("tests/inriasuite/inriasuite.pl", function()
               {
                   if (!env.execute(new Prolog.AtomTerm("run_all_tests")))
                       console.log("Failed");
                   else
                       console.log("Succeeded");
               });
