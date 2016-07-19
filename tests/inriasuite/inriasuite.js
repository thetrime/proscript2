var Prolog = require('../../src/core');
var env = new Prolog.Environment();
var test_file = "inriasuite.pl";



env.consultFile(test_file, function()
               {
                   env.execute(new Prolog.AtomTerm("run_all_tests"),
                               function()
                               {
                                   console.log("Succeeded");
                               },
                               function()
                               {
                                   console.log("Failed");
                               },
                               function(e)
                               {
                                   console.log("Error: " + e.toString());
                               });
               });
