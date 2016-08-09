var functions = [];
var fTop = 17;


var Module = Module || {};
Module.define_foreign = function(name, fn)
{
    var pointer = fTop;
    functions[fTop] = fn;
    fTop++;
    var userPtr = allocate(intArrayFromString("user"), 'i8', ALLOC_NORMAL);
    var namePtr = allocate(intArrayFromString(name), 'i8', ALLOC_NORMAL);
    Module._define_foreign_predicate_js(Module._find_module(Module._MAKE_ATOM(userPtr)),
                                        Module._MAKE_FUNCTOR(Module._MAKE_ATOM(namePtr), fn.length),
                                        pointer);
};

function badger(a,b,c)
{
    console.log("BADGERS: " + term_to_string(a) + "," + term_to_string(b) + "," + term_to_string(c) + "(" + this + ")\n");
    return SUCCESS;
}

var YIELD = 3;
var FAIL = 0;
var SUCCESS = 1;

function sleep(duration)
{
    duration = _integer_data(duration);
    return YIELD;
};

Module.onRuntimeInitialized = function()
{
    Module._init_prolog();
    console.log("Hello from an initialized system!");
    Module.define_foreign("badger", badger);
    Module.define_foreign("sleep", sleep);


    var d0 = new Date().getTime();
    Module._do_test(); // foobar was exported
    var d1 = new Date().getTime();
    console.log("Execution time: " + (d1-d0) + "ms");
};


function _foreign_call(backtrack, name, arity, argp)
{
    //console.log("Hello from Javascript! " + backtrack + "," + name +"," + arity + "," + argp);
    var args = Array(arity);
    for (var i = 0; i < arity; i++)
    {
        args[i] = getValue(argp+(4*i), '*');
    }
    return functions[name].apply(backtrack, args);
}

function term_to_string(a)
{
    switch(_term_type(a))
    {
        case 1: // atom
        return atom_chars(a);
        case 2: // integer
        return _integer_data(a);
        case 4: // float
        return _float_data(a);
        case 127: // Var
        return "_";
        case 128: // term
        {
            var ftor = atom_chars(_term_functor_name(a));
            var arity = _term_functor_arity(a);
            var output = ftor + "(";
            for (var i = 0; i < arity; i++)
            {
                output += term_to_string(_term_arg(a, i));
                if (i + 1 < arity)
                    output += ",";
            }
            return output + ")";
        }
        case 129: // pointer
        return "#" + a;
        case 3: // functor
        case 5: // bigint
        case 6: // rational
        case 7: // blob
        throw new Error("Unhandled term type");

        default:
        throw new Error("Non-existent term type?");
    }

}

function atom_chars(a)
{
    var length = _atom_length(a);
    var ptr = _atom_data(a);
    var chunk_size = 0x8000;
    if (length > chunk_size)
    {
        var chunks = [];
        for (var i = 0; i < length; i += chunk_size)
        {
            var chunk_length = chunk_size;
            if (chunk_length + i > length)
                chunk_length = length - i;
            c.push(String.fromCharCode.apply(null, HEAP8.subarray(ptr+i, ptr+i+chunk_length)));
        }
        return c.join("");
    }
    return String.fromCharCode.apply(null, HEAP8.subarray(ptr, ptr+length));
}
