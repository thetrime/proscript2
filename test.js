// Note that to run this you will need
//   FILESYSTEM=--embed-file src/builtin.pl --embed-file test.pl --embed-file tests
// in Makefile, otherwise you will get an error saying that run_all_tests/0 is undefined

onPrologReady = function(Prolog)
{
    Prolog._do_test();
}

require('./proscript');
