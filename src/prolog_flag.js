function flag_bounded(set, value)
{
    if (set) return permission_error("prolog_flag");
    return unify(value, lookup_atom("true"));
}

function flag_max_integer(set, value)
{
    if (set) return permission_error("prolog_flag");
    return unify(value, (268435455) ^ (TAG_INT<<WORD_BITS));
}

function flag_min_integer(set, value)
{
    if (set) return permission_error("prolog_flag");
    return unify(value, (536870911) ^ (TAG_INT<<WORD_BITS));
}

function flag_integer_rounding_function(set, value)
{
    if (set) return permission_error("prolog_flag");
    return unify(value, lookup_atom("toward_zero"));
}

function flag_char_conversion(set, value)
{
    if (set) 
    {
        if (TAG(value) == TAG_ATM && atable[VAL(value)] == "on")
            prolog_flag_values.char_conversion = true;
        else if (TAG(value) == TAG_ATM && atable[VAL(value)] == "off")
            prolog_flag_values.char_conversion = false;
        else
            return type_error("flag_value", value);
        return true;
    }
    return unify(value, prolog_flag_values.char_conversion?lookup_atom("on"):lookup_atom("off"));
}

function flag_debug(set, value)
{
    if (set) 
    {
        if (TAG(value) == TAG_ATM && atable[VAL(value)] == "on")
            prolog_flag_values.debug = true;
        else if (TAG(value) == TAG_ATM && atable[VAL(value)] == "off")
            prolog_flag_values.debug = false;
        else
        {
            return type_error("flag_value", value);
        }
        return true;
    }
    return unify(value, prolog_flag_values.debug?lookup_atom("on"):lookup_atom("off"));
}

function flag_max_arity(set, value)
{
    if (set) return permission_error("prolog_flag");
    return unify(value, lookup_atom("unbounded"));
}

function flag_unknown(set, value)
{
    if (set) 
    {
        if (TAG(value) == TAG_ATM && atable[VAL(value)] == "error")
            prolog_flag_values.unknown = "error";
        else if (TAG(value) == TAG_ATM && atable[VAL(value)] == "fail")
            prolog_flag_values.unknown = "fail";
        else if (TAG(value) == TAG_ATM && atable[VAL(value)] == "warning")
            prolog_flag_values.unknown = "warning";
        else
            return type_error("flag_value", value);
        return true;
    }
    return unify(value, lookup_atom(prolog_flag_values.unknown));
}

function flag_double_quotes(set, value)
{
    if (set) 
    {
        if (TAG(value) == TAG_ATM && atable[VAL(value)] == "chars")
            prolog_flag_values.double_quotes = "chars";
        else if (TAG(value) == TAG_ATM && atable[VAL(value)] == "codes")
            prolog_flag_values.double_quotes = "codes";
        else if (TAG(value) == TAG_ATM && atable[VAL(value)] == "atom")
            prolog_flag_values.double_quotes = "atom";
        else
            return type_error("flag_value", value);
        return true;
    }
    return unify(value, lookup_atom(prolog_flag_values.double_quotes));
}

function predicate_set_prolog_flag(key, value)
{
    if (TAG(key) != TAG_ATM)
        return type_error("atom", key);
    keyname = atable[VAL(key)];    
    
    for (var i = 0; i < prolog_flags.length; i++)
    {
        if (prolog_flags[i].name == keyname)
        {
            return prolog_flags[i].fn(true, value);
        }
    }
    debug("No such flag");
    return false;
}

function predicate_current_prolog_flag(key, value)
{
    if (TAG(key) == TAG_REF)
    {
        if (state.foreign_retry)
        {
            index = state.foreign_value + 1;         
        }
        else
        {
            create_choicepoint();
            index = 0;
        }
        update_choicepoint_data(index);        
        if (index >= prolog_flags.length)
        {
            destroy_choicepoint();
            return false;
        }
        unify(key, lookup_atom(prolog_flags[index].name));
        return prolog_flags[index].fn(false, value);        
    }
    else if (TAG(key) == TAG_ATM)
    {
        keyname = atable[VAL(key)];    
        var index = 0;
        for (var i = 0; i < prolog_flags.length; i++)
        {
            if (prolog_flags[index].name == keyname)
                return prolog_flags[index].fn(false, value);
        }
        return false;
    }
    else
        return type_error("atom", key);
}

var prolog_flags = [{name:"bounded", fn:flag_bounded},
                    {name:"max_integer", fn:flag_max_integer},
                    {name:"min_integer", fn:flag_min_integer},
                    {name:"integer_rounding_function", fn:flag_integer_rounding_function},
                    {name:"char_conversion", fn:flag_char_conversion},
                    {name:"debug", fn:flag_debug},
                    {name:"max_arity", fn:flag_max_arity},
                    {name:"unknown", fn:flag_unknown},
                    {name:"double_quotes", fn:flag_double_quotes}];

var prolog_flag_values = {char_conversion: false,
                          debug: false,
                          unknown: "error",
			  double_quotes: "codes"};

module.exports = {flags: prolog_flags,
		  values: prolog_flag_values};
