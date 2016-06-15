
function read_term(stream, term, options)
{
    if (!(options = parse_term_options(options)))
        return false;
    var streamindex = VAL(get_arg(stream, 1));
    var s = streams[streamindex];
    var context = {stream:s, peeked_token: undefined};
    var expression = {};
    if (!read_expression(context, Infinity, false, false, expression))
        return false;
    expression = expression.value;
    // Depending on the situation, we may expect a . now on the stream. 
    // There will not be one if we are going to return end_of_file because it is actually the eof
    // (Of course, if the file contains end_of_file. then we will return end_of_file AND read the .
    // Luckily we can distinguish the two cases
    // There will also not be one if we are in atom_to_term mode, which is not yet implemented    
    if (expression.end_of_file === undefined)
    {
        var period = {};
        if (!read_token(context, period))
            return false;
        if (period.value != ".") // Missing period === eof
            return syntax_error("end_of_file");
    }
    else
        expression = "end_of_file";
    debug_msg("Read expression: " + expression_to_string(expression));
    
    var varmap = {};
    var singletons = {};
    t1 = expression_to_term(expression, varmap, singletons);
    var rc = 1;
    if (options.variables !== undefined || options.singletons !== undefined)
    {
        var equals2 = lookup_functor("=", 2);
        var keys = Object.keys(varmap);
        for (var i = 0; i < keys.length; i++)
        {
            var varname = keys[i];
            if (options.variables !== undefined)
            {                
                if (!unify(state.H ^ (TAG_LST << WORD_BITS), options.variables))
                    return false;
                memory[state.H] = varmap[varname];
                memory[state.H+1] = (state.H+1) ^ (TAG_REF << WORD_BITS);
                options.variables = memory[state.H+1];
                state.H+=2;
            }
            if (options.variable_names !== undefined)
            {                
                if (!unify(state.H ^ (TAG_LST << WORD_BITS), options.variable_names))
                {
                    debug("not unifiable: " + term_to_string(options.variable_names));
                    return false;
                }
                memory[state.H] = (state.H+2) ^ (TAG_STR << WORD_BITS);
                memory[state.H+1] = (state.H+1) ^ (TAG_REF << WORD_BITS);
                options.variable_names = memory[state.H+1];
                memory[state.H+2] = equals2;
                memory[state.H+3] = lookup_atom(varname);
                memory[state.H+4] = varmap[varname];
                state.H+=5;
            }
        }
        if (options.variables !== undefined)
            if (!unify(options.variables, NIL))
                return false;
        if (options.variable_names !== undefined)
            if (!unify(options.variable_names, NIL))
                return false;       
    }
    if (options.singletons !== undefined)
    {
        var keys = Object.keys(singletons);
        for (var i = 0; i < keys.length; i++)
        {
            var varname = keys[i];
            if (singletons[varname] == 1)
            {
                if (!unify(state.H ^ (TAG_LST << WORD_BITS), options.singletons))
                    return false;
                memory[state.H] = (state.H+2) ^ (TAG_STR << WORD_BITS);
                memory[state.H+1] = (state.H+1) ^ (TAG_REF << WORD_BITS);
                options.singletons = memory[state.H+1];
                memory[state.H+2] = equals2;
                memory[state.H+3] = lookup_atom(varname);
                memory[state.H+4] = varmap[varname];
                state.H+=5;
            }
        }
        if (!unify(options.singletons, NIL))
            return false;      
    }
    debug_msg("A term has been created ( " + VAL(t1) + " ). Reading it back from the heap gives: " + term_to_string(t1));
    return unify(term, t1);
}



function predicate_write_term(stream, term, options)
{
    if (!(options = parse_term_options(options)))
        return false;
    var value = format_term(term, options);
    var s = {};
    if (!get_stream(stream, s))
        return false;
    s = s.value;
    if (s.write == null)
        return permission_error("output", "stream", stream);
    
    var bytes = IO.toByteArray(value);
    return (s.write(s, 1, bytes.length, bytes) >= 0)
}

function escape_atom(a)
{
    chars = a.split('');
    var result = "";
    for (var i = 0; i < chars.length; i++)
    {
        if (chars[i] == "'")
            result += "\\'";
        else
            result += chars[i];       
    }
    return result;
}

function quote_atom(a)
{
    if (a.charAt(0) >= "A" && a.charAt(0) <= "Z")
        return "'" + escape_atom(a) + "'";
    chars = a.split('');
    if (is_punctuation(chars[0]))
    {
        for (var i = 0; i < chars.length; i++)
        {
            if (!is_punctuation(chars[i]))
                return "'" + escape_atom(a) + "'";
        }
    }
    else
    {
        for (var i = 0; i < chars.length; i++)
        {
            if (is_punctuation(chars[i]) || chars[i] == ' ')
                return "'" + escape_atom(a) + "'";
        }
    }
    return a;
}


function is_operator(ftor)
{
    ftor = VAL(ftor);
    if (ftable[ftor][1] == 2 && infix_operators[atable[ftable[ftor][0]]] != undefined)
        return true;
    if (ftable[ftor][1] == 1 && prefix_operators[atable[ftable[ftor][0]]] != undefined)
        return true;
    return false;
}


function format_term(value, options)
{
    if (value == undefined)
        abort("Illegal memory access in format_term: " + hex(value) + ". Dumping...");
    value = deref(value);
    switch(TAG(value))
    {
    case TAG_REF:
        if (VAL(value) > HEAP_SIZE)
        {
            if (state.E > state.B)
                lTop = state.E + state.CP.code[state.CP.offset - 1] + 2;
            else
                lTop = state.B + memory[state.B] + 8;
            return "_L" + (lTop - VAL(value));
        }
        else
            return "_G" + VAL(value);
    case TAG_ATM:
        atom = atable[VAL(value)];
        if (atom == undefined)
            abort("No such atom: " + VAL(value));
        if (options.quoted === true)
            return quote_atom(atom);
        return atom;
    case TAG_INT:
        if ((VAL(value) & (1 << (WORD_BITS-1))) == (1 << (WORD_BITS-1)))
            return (VAL(value) - (1 << WORD_BITS)) + "";
        else
            return VAL(value) + "";
        // fall-through
    case TAG_FLT:
        return floats[VAL(value)] + "";
    case TAG_STR:
        var ftor = VAL(memory[VAL(value)]);
        if (options.numbervars === true && ftor == lookup_functor('$VAR', 1) && TAG(memory[VAL(value)+1]) == TAG_INT)
        {
            var index = VAL(memory[VAL(value)+1]);
            var result = String.fromCharCode(65 + (index % 26));
            if (index >= 26)
                result = result + Math.floor(index / 26);
            return result;
        }
        if (!is_operator(ftor) || options.ignore_ops === true)
        {
            // Print in canonical form functor(arg1, arg2, ...)
            var result = format_term(ftable[ftor][0] ^ (TAG_ATM << WORD_BITS), options) + "(";
            for (var i = 0; i < ftable[ftor][1]; i++)
            {
                result += format_term(memory[VAL(value)+1+i], options);
                if (i+1 < ftable[ftor][1])
                    result += ",";
            }
            return result + ")";            
        }
        else
        {
            // Print as an operator
            var fname = atable[ftable[ftor][0]];
            if (ftable[ftor][1] == 2 && infix_operators[fname] != undefined)
            {
                // Infix operator
                var lhs = format_term(memory[VAL(value)+1], options);
                if (is_punctuation(lhs.charAt(lhs.length-1)) && !is_punctuation(fname.charAt(0)))
                    result = lhs + fname;
                else if (!is_punctuation(lhs.charAt(lhs.length-1)) && is_punctuation(fname.charAt(0)))
                    result = lhs + fname;
                else
                {
                    result = lhs + " " + fname;
                }
                var rhs = format_term(memory[VAL(value)+2], options);
                if (is_punctuation(rhs.charAt(0)) && !is_punctuation(fname.charAt(fname.length-1)))
                    return result + rhs;
                else if (!is_punctuation(rhs.charAt(0)) && is_punctuation(fname.charAt(fname.length-1)))
                    return result + rhs;
                else
                    return result + " " + rhs;
            }
            else if (ftable[ftor][1] == 1 && prefix_operators[fname] != undefined)
            {
                // Prefix operator
                var rhs = format_term(memory[VAL(value)+1], options);
                if (is_punctuation(rhs.charAt(0)) && !is_punctuation(fname.charAt(fname.length-1)))
                    return fname + rhs;
                else if (!is_punctuation(rhs.charAt(0)) && is_punctuation(fname.charAt(fname.length-1)))
                    return fname + rhs;
                else
                    return fname + " " + rhs;

            }
        }
    case TAG_LST:
        if (options.ignore_ops)
            return "'.'(" + format_term(memory[VAL(value)], options) + "," + format_term(memory[VAL(value)+1], options) + ")";
        // Otherwise we need to print the list in list-form
        var result = "[";
        var head = memory[VAL(value)];
        var tail = memory[VAL(value)+1];
        while (true)
        {
            result += format_term(head, options);
            if (tail == NIL)
                return result + "]";
            else if (TAG(tail) == TAG_LST)
            {
                head = memory[VAL(tail)];
                tail = memory[VAL(tail)+1];
                result += ",";
            }
            else 
                return result + "|" + format_term(tail, options) + "]";
        }        
    }
}
