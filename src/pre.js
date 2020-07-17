// FIXME: writeStringToMemory is unsafe and anywhere it is used there is a chance our unicode support is wrong!

/* This comprises the Javascript end of the FLI */
//var NodeXMLHttpRequest = require('xmlhttprequest').XMLHttpRequest;
var NodeXMLHttpRequest = XMLHttpRequest;

var functions = [];
var fTop = 0;

var callbacks = [];
var cTop = 0;

var cleanups = {};
var next_cleanup = 0;

var environments = [];
var eTop = 0;

// FIXME: This assumes 32 bit. Currently asm.js is definitely 32 bit, but maybe not one day? We could get this at startup by calling a C function to get sizeof(uintptr_t)
var SIZE_OF_WORD = 4;


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


function _foreign_call(backtrack, name, arity, argp)
{
    //console.log("Hello from Javascript! " + backtrack + "," + name +"," + arity + "," + argp);
    var args = Array(arity);
    for (var i = 0; i < arity; i++)
    {
        args[i] = _DEREF(getValue(argp+(SIZE_OF_WORD*i), '*'));
    }
    var context = environments[eTop-1];
    context.foreign = backtrack;
    if (functions[name] === undefined)
    {
        console.log("Cannot find function for " + name);
        console.log("fTop is " + fTop);
    }
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
    var length = __atom_length(a);
    var ptr = __atom_data(a);
    return read_string(ptr, length);
}

function _numeric_value(a)
{
    a = _DEREF(a);
    switch(_term_type(a))
    {
        case 2: // integer
        return _integer_data(a);
        case 4: // float
        return _float_data(a);
        case 5: // bigint
        case 6: // rational
        default:
        return undefined;
    }
}


function read_string(ptr, length)
{
    var chunk_size = 0x8000;
    if (length > chunk_size)
    {
        var chunks = [];
        for (var i = 0; i < length; i += chunk_size)
        {
            var chunk_length = chunk_size;
            if (chunk_length + i > length)
                chunk_length = length - i;
            chunks.push(String.fromCharCode.apply(null, HEAP8.subarray(ptr+i, ptr+i+chunk_length)));
        }
        return chunks.join("");
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

function _is_blob(a, type)
{
    a = _DEREF(a);
    if ((a & TAG_MASK) == CONSTANT_TAG)
    {
        var ptr = _cmalloc(type.length+1);
        writeStringToMemory(type, ptr);
        var result = __is_blob(a, ptr);
        _cfree(ptr);
        return result;
    }
    return false;
}

function _is_variable(a)
{
    a = _DEREF(a);
    return (a & TAG_MASK) == VARIABLE_TAG;
}

var blobs = {};
var next_free = {};

function portray_js_blob(typeptr, typelen, index, options, precedence, lenptr)
{
    var type = read_string(typeptr, typelen);
    var b = blobs[type][index];
    if (b.portray === undefined)
        return 0;
    var data = b.portray(options, precedence)
    setValue(lenptr, data.length, '*');
    var ptr = _cmalloc(data.length+1);
    writeStringToMemory(data, ptr);
    return ptr;
}

function _make_blob(type, object)
{
    var key;
    if (blobs[type] == undefined)
    {
        blobs[type] = [object];
        next_free[type] = -1;
        key = 0;
    }
    else
    {
        if (next_free[type] == -1)
            key = blobs[type].length-1;
        else
        {
            key = next_free[type];
            // FIXME: Shouldnt this be next_free[type] = (blobs[type])[key]; ?
            next_free[type] = blobs[key];
        }
        blobs[type].push(object);
        key = blobs[type].length-1;
    }
    var ptr = _cmalloc(type.length+1);
    writeStringToMemory(type, ptr);
    var result = __make_blob_from_index(ptr, key);
 //   console.log("Created blob at location " + key + " of type " + type + " with prolog handle " + result);
    _cfree(ptr);
    return result;
}

function _release_blob(type, w)
{
    w = _DEREF(w);
    var ptr = _cmalloc(type.length+1);
    writeStringToMemory(type, ptr);
    var key = __release_blob(ptr, w);
    _cfree(ptr);
    // Finally, excise the blob from the blobs structure
    // We cannot just splice the array (for one thing, it is inefficient. For another thing, it would change the indices of all the higher elements)
    // Instead, insert a tombstone so we can recycle this spot later
//    console.log("Destroying blob with handle " + w);
    blobs[type][key] = next_free[type];
    next_free[type] = key;
}

function _make_functor(name, arity)
{
    return __make_functor(name, arity);
}

function _make_compound(functor, args)
{
    if (!_is_functor(functor))
        functor = _make_functor(functor, args.length);
    var ptr = _cmalloc(args.length * SIZE_OF_WORD);
    for (var i = 0; i < args.length; i++)
        setValue(ptr + (SIZE_OF_WORD*i), args[i], '*');
    var w = __make_compound(functor, ptr);
    _cfree(ptr);
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

function _hard_reset()
{
    __hard_reset();
}

function _do_test()
{
    __do_test();
}


function _copy_term(a)
{
    return ___copy_term(a);
}

function _get_blob(type, w)
{
    w = _DEREF(w);
    var ptr = _cmalloc(type.length+1);
    writeStringToMemory(type, ptr);
    var key = __get_blob(ptr, w);
    _cfree(ptr);
    //console.log("Blob " + w + " of type " + w + " should be at location " + key);
    return blobs[type][key];
}

function _make_atom(a)
{
    var ptr = _cmalloc(a.length+1);
    writeStringToMemory(a, ptr);
    var atom = __make_atom(ptr, a.length);
    _cfree(ptr);
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
    var ptr = _cmalloc(a.length+1);
    writeStringToMemory(a, ptr);
    __consult_string(ptr);
    _cfree(ptr);
}


function _consult_url(url, callback)
{
    var xhr = new NodeXMLHttpRequest();
    xhr.open('get', url, true);
    xhr.onload = function()
    {
	var status = xhr.status;
	if (status == 200)
        {
            _consult_string(xhr.responseText);
            console.log("Successfully consulted " + url);
            callback(true, status);
	}
	else
	{
	    // FIXME: Do something better here?
	    console.log("Failed to consult " + url);
            callback(false, status);
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

function _call(context, goal)
{
    environments[eTop++] = context;
    var result = _calljs(goal);
    environments[--eTop] = null;
    return result;
}

function _execute(context, goal, callback)
{
    var pointer = cTop;
    callbacks[cTop++] = callback;
    environments[eTop++] = context;
    //console.log(_format_term(null, 1200, goal));
    _executejs(goal, pointer);
}

function xunify(a, b)
{
    return _safe_unify(a, b);
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

function _make_choicepoint(a)
{
    a = _DEREF(a);
    return __make_choicepoint(a);
}

function _jscleanup(index, arg)
{
    cleanups[index](arg);
    cleanups[index] = undefined;
}

function _make_choicepoint_with_cleanup(a, callback)
{
    var index = next_cleanup;
    next_cleanup++;
    cleanups[index] = callback;
    return __make_choicepoint_with_cleanup(_DEREF(a), index);
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
var portray_options = null;

function _format_term(options, priority, term)
{
    var lenptr = _cmalloc(SIZE_OF_WORD);
    var strptr = _cmalloc(SIZE_OF_WORD);
    if (options == null)
    {
        if (default_options == null)
            default_options = __create_options();
        options = default_options;
    }
    __format_term(options, priority, term, strptr, lenptr);
    var ptr = getValue(strptr, '*');
    var result = UTF8ToString(ptr, getValue(lenptr, '*'));
    _cfree(ptr);
    _cfree(lenptr);
    _cfree(strptr);
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

function _free_options(options)
{
    return __free_options(options);
}

// If you pass in a constant to make_local, the same constant is returned
// Calling free_local() on a constant has no effect. This simplifies a lot of
// code so that you dont have to check the type before saving a word somewhere
// in foreign code
function make_local(t)
{
    t = _DEREF(t);
    if ((_DEREF(t) & TAG_MASK) == CONSTANT_TAG)
        return t;
    var ptr = _cmalloc(SIZE_OF_WORD);
    _copy_local(t, ptr);
    var value = getValue(ptr, '*');
    _cfree(ptr);
    return value;
}

function free_local(t)
{
    // For an explanation of why this works, see the comment in module.c in the definition of asserta.
    // It is CRITICAL that we do not try and dereference t before freeing it, otherwise the trick will
    // not work and we will try to free the middle of a block!
    if ((_DEREF(t) & TAG_MASK) == CONSTANT_TAG)
        return; // The copy was never a copy to begin with
    return _cfree(t);
}

function portray(t)
{
    return _format_term(null, 1200, t);
}

// WARNING: This may return 0 in the event of a parse error!
function string_to_local_term(str)
{
    var requiredSpace = lengthBytesUTF8(str);
    var ptr = _cmalloc(requiredSpace+1); // Plus a NUL added by stringToUTF8
    stringToUTF8(str, ptr, requiredSpace+1); // Have to include space for the NUL here too or our output gets truncated by it
    var result = __string_to_local_term(ptr, requiredSpace);
    _cfree(ptr);
    return result;
}

// WARNING: Calling this on NULL will produce undefined behaviour!
function deref(t)
{
    return _DEREF(t);
}

function forEach(t, i, e)
{
    var list = t;
    while (_is_list(list))
    {
        i(_list_head(list));
        list = _list_tail(list);
    }
    if (!_is_empty_list(list))
        e(list);
}

module.exports = {make_atom: _make_atom,
                  make_functor: _make_functor,
                  make_variable: _make_variable,
                  make_compound: _make_compound,
                  make_choicepoint: _make_choicepoint,
                  make_choicepoint_with_cleanup: _make_choicepoint_with_cleanup,
                  make_integer: _make_integer,
                  make_float: _make_float,
                  make_blob: _make_blob,
                  release_blob: _release_blob,
                  is_constant: _is_constant,
                  is_functor: _is_functor,
                  is_variable: _is_variable,
                  is_compound: _is_compound,
                  is_integer: _is_integer,
                  is_float: _is_float,
                  is_atom: _is_atom,
                  is_blob: _is_blob,
                  atom_chars: _atom_chars,
                  get_blob: _get_blob,
                  term_functor: _term_functor,
                  term_functor_arity: _term_functor_arity,
                  term_functor_name: _term_functor_name,
                  term_arg: _term_arg,
                  consult_url: _consult_url,
                  consult_string: _consult_string,
                  exists_predicate: _exists_predicate,
                  get_exception: _get_exception,
                  clear_exception: _clear_exception,
                  set_exception: _set_exception,
                  define_foreign: define_foreign,
                  save_state: _save_state,
                  restore_state: _restore_state,
                  execute: _execute,
                  call: _call,
                  unify: xunify,
                  format_term: _format_term,
                  create_options: _create_options,
                  set_option: _set_option,
                  free_options: _free_options,
                  _yield: yield_resumption,
                  make_local: make_local,
                  copy_term: _copy_term,
                  free_local: free_local,
                  string_to_local_term: string_to_local_term,
                  portray: portray,
                  forEach: forEach,
                  numeric_value: _numeric_value,
                  deref: deref,
                  hard_reset: _hard_reset
                 };

/* This is the ACTUAL preamble */
//var Module = Module || {};

if (typeof proscriptPrefixURL !== 'undefined')
    Module.memoryInitializerPrefixURL = proscriptPrefixURL;

Module.onRuntimeInitialized = function()
{
    Module._init_prolog();
    //console.log("Hello from an initialized system!");
    if (typeof onPrologReady == 'function')
        onPrologReady(module.exports);
    prologReady = true;
};
