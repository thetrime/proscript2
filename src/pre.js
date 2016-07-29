var functions = [];
var fTop = 17;


var Module =
    {
        define_foreign: function(name, fn)
        {
            var pointer = fTop;
            functions[fTop] = fn;
            fTop++;
            var userPtr = allocate(intArrayFromString("user"), 'i8', ALLOC_NORMAL);
            var namePtr = allocate(intArrayFromString(name), 'i8', ALLOC_NORMAL);
            Module._define_foreign_predicate_js(Module._find_module(Module._MAKE_ATOM(userPtr)),
                                                Module._MAKE_FUNCTOR(Module._MAKE_ATOM(namePtr), fn.length),
                                                pointer);
        },

        onRuntimeInitialized: function()
        {
            Module._init_prolog();
            var badger = function(a)
            {
                console.log("BADGERS\n");
            }
            console.log("Hello from an initialized system!");
            Module.define_foreign("badger", badger);
            Module._do_test(); // foobar was exported
        }
    };


function _foreign_call(foreign, name, arity, argp)
{
    console.log("Hello from Javascript! " + foreign + "," + name +"," + arity + "," + argp);
    return 0;
}

