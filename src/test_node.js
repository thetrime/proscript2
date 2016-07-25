GLOBAL.window = GLOBAL;

var Prolog = require('./core');
var env = new Prolog.Environment();
var test_file = "tests/r.pl";
env.consultFile(test_file, function()
                {
                    var d0 = Date.now();
                    env.execute(Prolog.AtomTerm.get("run_all_tests"),
                                function()
                                {
                                    console.log("Succeeded");
                                    var d1 = Date.now();
                                    console.log("Execution (not compilation) time: " + (d1 - d0)/1000 + "s");
                                },
                                function()
                                {
                                    console.log("Failed");
                                    var d1 = Date.now();
                                    console.log("Execution (not compilation) time: " + (d1 - d0)/1000 + "s");
                                },
                                function(error)
                                {
                                    console.log("Raised " + PORTRAY(error));
                                    var d1 = Date.now();
                                    console.log("Execution (not compilation) time: " + (d1 - d0)/1000 + "s");
                                });
                });
