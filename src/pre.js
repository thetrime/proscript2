/* This comprises the Javascript end of the FLI */
var XMLHttpRequest = require('xmlhttprequest').XMLHttpRequest;

var functions = [];
var fTop = 0;

var callbacks = [];
var cTop = 0;

var environments = [];
var eTop = 0;


function define_foreign(name, fn)
{
    var pointer = fTop;
    functions[fTop++] = fn;
    var userPtr = allocate(intArrayFromString("user"), 'i8', ALLOC_NORMAL);
    var namePtr = allocate(intArrayFromString(name), 'i8', ALLOC_NORMAL);
    _define_foreign_predicate_js(_find_module(_MAKE_ATOM(userPtr)),
                                 _MAKE_FUNCTOR(_MAKE_ATOM(namePtr), fn.length),
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
var TAG_MASK = 3;
var VARIABLE_TAG = 0;
var POINTER_TAG = 1;
var COMPOUND_TAG = 2;
var CONSTANT_TAG = 3;

function yield_resumption()
{
    var ptr = _current_yield();
    return function(p)
    {
        _resume_yield(p, ptr);
    };
}

function sleep(duration)
{
    duration = _integer_data(duration);
    //var resume = yield_resumption();
    console.log("Goodnight: " + duration * 1000);
    setTimeout(function()
               {
                   _resume_yield(SUCCESS);
               }, duration * 1000);
    return YIELD;
};


function _foreign_call(backtrack, name, arity, argp)
{
    //console.log("Hello from Javascript! " + backtrack + "," + name +"," + arity + "," + argp);
    var args = Array(arity);
    for (var i = 0; i < arity; i++)
    {
        args[i] = getValue(argp+(4*i), '*');
    }
    var context = environments[eTop-1];
    context.foreign = backtrack;
    return functions[name].apply(context, args);
}

function term_to_string(a)
{
    a = _DEREF(a);
    switch(_term_type(a))
    {
        case 1: // atom
        return _atom_chars(a);
        case 2: // integer
        return _integer_data(a);
        case 4: // float
        return _float_data(a);
        case 127: // Var
        return "_";
        case 128: // term
        {
            var ftor = _atom_chars(_term_functor_name(a));
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

function _atom_chars(a)
{
    a = _DEREF(a);
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

function _is_compound(a)
{
    a = _DEREF(a);
    return (a & TAG_MASK) == COMPOUND_TAG;
}

function _is_constant(a)
{
    a = _DEREF(a);
    return (a & TAG_MASK) == CONSTANT_TAG;
}

function _is_atom(a)
{
    a = _DEREF(a);
    return (a & TAG_MASK) == CONSTANT_TAG && _term_type(a) == 1;
}

function _is_integer(a)
{
    a = _DEREF(a);
    return (a & TAG_MASK) == CONSTANT_TAG && _term_type(a) == 2;
}

function _is_functor(a)
{
    a = _DEREF(a);
    return (a & TAG_MASK) == CONSTANT_TAG && _term_type(a) == 3;
}

function _is_float(a)
{
    a = _DEREF(a);
    return (a & TAG_MASK) == CONSTANT_TAG && _term_type(a) == 4;
}

function _is_blob(a)
{
    a = _DEREF(a);
    return (a & TAG_MASK) == CONSTANT_TAG && _term_type(a) == 7;
}

function _is_variable(a)
{
    a = _DEREF(a);
    return (a & TAG_MASK) == VARIABLE_TAG;
}

var blobs = {};

function _make_blob(type, object)
{
    var key;
    if (blobs[type] == undefined)
    {
        blobs[type] = [object];
        key = 0;
    }
    else
    {
        blobs[type].push(object);
        key = blobs[type].length-1;
    }
    //console.log("Created blob " + key + " of type " + type + ": " + blobs[type][key]);
    var ptr = _malloc(type.length+1);
    writeStringToMemory(type, ptr);
    var result = __make_blob(ptr, key);
    _free(ptr);
    return result;
}

function _make_functor(name, arity)
{
    return __make_functor(name, arity);
}

function _make_compound(functor, args)
{
    if (!_is_functor(functor))
        functor = _make_functor(functor, args.length);
    // FIXME: This assumes 32 bit. Currently asm.js is definitely 32 bit, but maybe not one day? We could get this at startup by calling a C function to get sizeof(uintptr_t)
    var SIZE_OF_WORD = 4;
    var ptr = _malloc(args.length * SIZE_OF_WORD);
    for (var i = 0; i < args.length; i++)
        setValue(ptr + (SIZE_OF_WORD*i), args[i], '*');
    var w = __make_compound(functor, ptr);
    _free(ptr);
    return w;
}

function _save_state()
{
    return __push_state();
}

function _restore_state(a)
{
    __restore_state(a);
}

function _get_blob(type, w)
{
    w = _DEREF(w);
    var ptr = _malloc(type.length+1);
    writeStringToMemory(type, ptr);
    var key = __get_blob(ptr, w);
    _free(ptr);
    //console.log("Looking up blob " + key + " of type " + type + ": " + blobs[type][key]);
    return blobs[type][key];
}

function _make_atom(a)
{
    var ptr = _malloc(a.length+1);
    writeStringToMemory(a, ptr);
    var atom = __make_atom(ptr, a.length);
    _free(ptr);
    return atom;
}


function _make_integer(a)
{
    return _MAKE_INTEGER(a);
}

function _make_float(a)
{
    return _MAKE_FLOAT(a);
}

function _make_variable(a)
{
    return _MAKE_VAR(a);
}


function _consult_string(a)
{
    var ptr = _malloc(a.length+1);
    writeStringToMemory(a, ptr);
    __consult_string(ptr);
    _free(ptr);
}


function _consult_url(url, callback)
{
    var xhr = new XMLHttpRequest();
    xhr.open('get', url, true);
    xhr.onload = function()
    {
	var status = xhr.status;
	if (status == 200)
        {
            _consult_string(xhr.responseText);
	    callback();
	}
	else
	{
	    // FIXME: Do something better here?
	    console.log("Failed to consult " + url);
	    console.log(status);
	}
    };
    xhr.send();
}

function _exists_predicate(a, b)
{
    return __exists_predicate(a, b);
}

function _get_exception()
{
    return __get_exception();
}

function _set_exception(a)
{
    return __set_exception(a);
}

function _clear_exception()
{
    __clear_exception();
}

function _jscallback(result)
{
    callbacks[--cTop](result);
    environments[--eTop] = null;
}

function _execute(context, goal, callback)
{
    var pointer = cTop;
    callbacks[cTop++] = callback;
    environments[eTop++] = context;
    _executejs(goal, pointer);
}

function xunify(a, b)
{
    return _unify(a, b);
}

function _term_functor(a)
{
    a = _DEREF(a);
    return __term_functor(a);
}

function _term_functor_arity(a)
{
    a = _DEREF(a);
    return __term_functor_arity(a);
}

function _term_functor_name(a)
{
    a = _DEREF(a);
    return __term_functor_name(a);
}

function _term_arg(a, i)
{
    a = _DEREF(a);
    return _DEREF(__term_arg(a, i));
}

var default_options = null;

function _format_term(options, priority, term)
{
    var SIZE_OF_WORD = 4;
    var lenptr = _malloc(SIZE_OF_WORD);
    var strptr = _malloc(SIZE_OF_WORD);
    if (options == null)
    {
        if (default_options == null)
            default_options = __create_options();
        options = default_options;
    }
    __format_term(options, priority, term, strptr, lenptr);
    var ptr = getValue(strptr, '*');
    var result = Pointer_stringify(ptr, getValue(lenptr, '*'));
    _free(ptr);
    _free(lenptr);
    _free(strptr);
    return result;
}

function _create_options()
{
    return __create_options();
}

function _set_option(options, key, value)
{
    return __set_option(options, key, value);
}

function make_local(t)
{
    return _copy_local(t, 0);
}

function free_local(t)
{
    // For an explanation of why this works, see the comment in module.c in the definition of asserta
    return _free(t);
}


module.exports = {_make_atom: _make_atom,
                  _make_functor: _make_functor,
                  _make_variable: _make_variable,
                  _make_compound: _make_compound,
                  _make_integer: _make_integer,
                  _make_float: _make_float,
                  _make_blob: _make_blob,
                  _is_constant: _is_constant,
                  _is_functor: _is_functor,
                  _is_variable: _is_variable,
                  _is_compound: _is_compound,
                  _is_integer: _is_integer,
                  _is_float: _is_float,
                  _is_atom: _is_atom,
                  _is_blob: _is_blob,
                  _atom_chars: _atom_chars,
                  _get_blob: _get_blob,
                  _term_functor: _term_functor,
                  _term_functor_arity: _term_functor_arity,
                  _term_functor_name: _term_functor_name,
                  _term_arg: _term_arg,
                  _consult_url: _consult_url,
                  _consult_string: _consult_string,
                  _exists_predicate: _exists_predicate,
                  _get_exception: _get_exception,
                  _clear_exception: _clear_exception,
                  _set_exception: _set_exception,
                  define_foreign: define_foreign,
                  _save_state: _save_state,
                  _restore_state: _restore_state,
                  _execute: _execute,
                  _unify: xunify,
                  _format_term: _format_term,
                  _create_options: _create_options,
                  _set_option: _set_option,
                  _yield: yield_resumption,
                  _make_local: make_local,
                  _free_local: free_local,
                 };

/* This is the ACTUAL preamble */
var Module = Module || {};

Module.onRuntimeInitialized = function()
{
    Module._init_prolog();
    /*
    console.log("Hello from an initialized system!");
    define_foreign("badger", badger);
    define_foreign("sleep", sleep);


    var d0 = new Date().getTime();
    Module._do_test(); // foobar was exported
    var d1 = new Date().getTime();
    console.log("Execution time: " + (d1-d0) + "ms");
    */
};
