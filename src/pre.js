var Module =
    {
        onRuntimeInitialized: function()
        {
            console.log("Hello from an initialized system!");
            Module._do_test(); // foobar was exported
        },
        q: require('./proscript.js')
    };


function _foreign_call(foreign, name, arity, argp)
{
    console.log("Hello from Javascript!");
    return 0;
}

