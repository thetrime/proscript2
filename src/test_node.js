var Prolog = require('./core');
var env = new Prolog.Environment();
var test_file = "tests/inriasuite/inriasuite.pl";
var test_file = "tests/t.pl";
env.consultFile(test_file, function()
               {
                   if (!env.execute(new Prolog.AtomTerm("run_all_tests")))
                       console.log("Failed");
                   else
                       console.log("Succeeded");
               });
