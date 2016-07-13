var Prolog = require('../../src/core');
var env = new Prolog.Environment();
var test_file = "inriasuite.pl";
env.consultFile(test_file, function()
               {
                   if (!env.execute(new Prolog.AtomTerm("run_all_tests")))
                       console.log("Failed");
                   else
                       console.log("Succeeded");
               });
