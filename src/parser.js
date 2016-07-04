/* Term reading */
// See http://journal.stuffwithstuff.com/2011/03/19/pratt-parsers-expression-parsing-made-easy/
// Parsers return either:
// 1) An string, in case of an atom
// 2) An integer, in case of an integer
// 3) An object with .list and .tail if a list (because apparently it is not easy to determine if the type of something is a list at runtime!?)
//      If it is a proper list, .tail == NIL
// 4) An object with .variable_name, if a variable
// 5) An object with .functor (a string) and .args (an array) defined if a term

var Stream = require('./stream.js');
var IO = require('./io.js');
var PrologFlag = require('./prolog_flag.js');
var CompoundTerm = require('./compound_term.js');
var VariableTerm = require('./variable_term.js');
var IntegerTerm = require('./integer_term.js');
var AtomTerm = require('./atom_term.js');
var Constants = require('./constants.js');

function parse_infix(s, lhs, precedence, vars)
{
    var token = read_token(s);
    var rhs = read_expression(s, precedence, false, false, vars);
    return new CompoundTerm(token, [lhs, rhs]);
}

function parse_postfix(s, lhs)
{
    var token = read_token(s)
    return new CompoundTerm(token, [lhs]);
}

function syntax_error(msg)
{
    // FIXME:
    throw msg;
}

function atomic(token)
{
    if (!isNaN(token) && parseInt(Number(token)) == token)
    {
        console.log("#############Integer: " + token);
        return new IntegerTerm(token);
    }
    // FIXME: Floats, bigintegers, rationals
    return new AtomTerm(token);
}

// A reminder: yfx means an infix operator f, with precedence p, where the lhs has a precendece <= p and the rhs has a precedence < p.

var prefix_operators = {":-": {precedence: 1200, fixity: "fx"},
                        "?-": {precedence: 1200, fixity: "fx"},
                        "dynamic": {precedence: 1150, fixity: "fx"},
                        "discontiguous": {precedence: 1150, fixity: "fx"},
                        "initialization": {precedence: 1150, fixity: "fx"},
                        "meta_predicate": {precedence: 1150, fixity: "fx"},
                        "module_transparent": {precedence: 1150, fixity: "fx"},
                        "multifile": {precedence: 1150, fixity: "fx"},
                        "thread_local": {precedence: 1150, fixity: "fx"},
                        "volatile": {precedence: 1150, fixity: "fx"},
                        "\\+": {precedence: 900, fixity: "fy"},
                        "~": {precedence: 900, fixity: "fx"},
                        "?": {precedence: 500, fixity: "fx"},
                        "+": {precedence: 200, fixity: "fy"},
                        "-": {precedence: 200, fixity: "fy"},
                        "\\": {precedence: 200, fixity: "fy"}};


var infix_operators = {":-": {precedence: 1200, fixity: "xfx"},
                       "-->": {precedence: 1200, fixity: "xfx"},
                       ";": {precedence: 1100, fixity: "xfy"},
                       "|": {precedence: 1100, fixity: "xfy"},
                       "->": {precedence: 1050, fixity: "xfy"},
                       "*->": {precedence: 1050, fixity: "xfy"},
                       ",": {precedence: 1000, fixity: "xfy"},
                       ":=": {precedence: 990, fixity: "xfx"},
                       "<": {precedence: 700, fixity: "xfx"},
                       "=": {precedence: 700, fixity: "xfx"},
                       "=..": {precedence: 700, fixity: "xfx"},
                       "=@=": {precedence: 700, fixity: "xfx"},
                       "=:=": {precedence: 700, fixity: "xfx"},
                       "=<": {precedence: 700, fixity: "xfx"},
                       "==": {precedence: 700, fixity: "xfx"},
                       "=\\=": {precedence: 700, fixity: "xfx"},
                       ">": {precedence: 700, fixity: "xfx"},
                       ">=": {precedence: 700, fixity: "xfx"},
                       "@<": {precedence: 700, fixity: "xfx"},
                       "@=<": {precedence: 700, fixity: "xfx"},
                       "@>": {precedence: 700, fixity: "xfx"},
                       "@>=": {precedence: 700, fixity: "xfx"},
                       "\\=": {precedence: 700, fixity: "xfx"},
                       "\\==": {precedence: 700, fixity: "xfx"},
                       "is": {precedence: 700, fixity: "xfx"},
                       ">:<": {precedence: 700, fixity: "xfx"},
                       ":<": {precedence: 700, fixity: "xfx"},
                       ":": {precedence: 600, fixity: "xfy"},
                       "+": {precedence: 500, fixity: "yfx"},
                       "-": {precedence: 500, fixity: "yfx"},
                       "/\\": {precedence: 500, fixity: "yfx"},
                       "\\/": {precedence: 500, fixity: "yfx"},
                       "xor": {precedence: 500, fixity: "yfx"},
                       "*": {precedence: 400, fixity: "yfx"},
                       "/": {precedence: 400, fixity: "yfx"},
                       "//": {precedence: 400, fixity: "yfx"},
                       "rdiv": {precedence: 400, fixity: "yfx"},
                       "<<": {precedence: 400, fixity: "yfx"},
                       ">>": {precedence: 400, fixity: "yfx"},
                       "mod": {precedence: 400, fixity: "yfx"},
                       "rem": {precedence: 400, fixity: "yfx"},
                       "**": {precedence: 200, fixity: "xfx"},
                       "^": {precedence: 200, fixity: "xfy"}};

// This returns a javascript object representation of the term. It takes the two extra args because of some oddities with Prolog:
// 1) If we are reading foo(a, b) and we are at the a, we would accept the , as part of the LHS. ie, we think (a,b) is the sole argument. Instead, we should make , have
//    very high precedence if we are reading an arg. Of course, () can reduce this again, so that foo((a,b)) does indeed read ,(a,b) as the single argument
// 2) | behaves slightly differently in lists, in a similar sort of way
function read_expression(s, precedence, isarg, islist, vars)
{
    var token = read_token(s);
    if (token == null)
    {
	return null; // end of file
    }
    var lhs;
    // Either the token is an operator, or it must be an atom (or the start of a list or curly-list)
    var op = prefix_operators[token];
    if (op === undefined)
    {  
        if (token == "\"")
        {
            // We have to just read chars until we get a close " (taking care with \" in the middle)
            var args = [];
            var t = 0;
            var mode = 0;
	    if (PrologFlag.values['double_quotes'] == "chars")
                mode = 0;
	    else if (PrologFlag.values['double_quotes'] == "codes")
                mode = 1;
	    else if (PrologFlag.values['double_quotes'] == "atom")
                mode = 2;
            while (true)
            {
                t = get_raw_char_with_conversion(s.stream);
                if (t == '"')
                    break;
                if (t == "\\")
                {
		    if (peek_raw_char_with_conversion(s.stream) == '"')
                    {
                        get_raw_char_with_conversion(s.stream);
                        if (mode == 1)
			    args.push(new IntegerTerm('"'.charCodeAt(0)));
                        else
			    args.push('"');
                        continue;
                    }
                }
                if (mode == 1)
		    args.push(new IntegerTerm(t.charCodeAt(0)));
                else
		    args.push(t);
            }
            if (mode == 2)
		lhs = new AtomTerm(args.join(''));
	    else
	    {
		lhs = make_list(args, Constants.emptyListAtom);
	    }
        }
        else if (token == "[" || token == "{")
        {
            // The principle for both of these is very similar
            var args = [];
            var next = {};
            while(true)
            {
		var t = read_expression(s, Infinity, true, true, vars);
		if (t == "]")
                {
                    lhs = "[]";
                    break;
                    // Special case for the empty list, since the first argument is ']'
                }
                args.push(t);
		var next = read_token(s);
                if (next == ',')
                    continue;
                else if (next == "]" && token == "[")
		{
		    lhs = make_list(args, Constants.emptyListAtom);
		    break;
                }
                else if (next == "}" && token == "{")
                {
		    lhs = new CompoundTerm("{}", args);
                    break;
                }
                else if (next == "|" && token == "[")
                {
		    var tail = read_expression(s, Infinity, true, true, vars);
		    lhs = make_list(args, tail);
		    var next = read_token(s);
		    if (next == "]")
                        break;
                    else
                        return syntax_error("missing ]");
                }
                else
                    return syntax_error("mismatched " + token + " at " + next);
            }
        }
        else if (token == "(")
        {
            // Is this right? () just increases the precedence to infinity and reads another term?
	    var lhs = read_expression(s, Infinity, false, false, vars)
	    var next = read_token(s);
            if (next != ")")
                return syntax_error("mismatched ( at " + next);
	}
	else if (token.variable_name != undefined)
	{
            // It is a variable
            if (token.variable_name.startsWith("_"))
                lhs = new VariableTerm();
            else
            {
                if (vars[token.variable_name] === undefined)
                    vars[token.variable_name] = new VariableTerm(token.variable_name);
                lhs = vars[token.variable_name];
            }
	}
        else
        {
            // It is an atomic term
            lhs = atomic(token);
	}
    }
    else if (op.fixity == "fx")
    {
	var arg = read_expression(s, op.precedence, isarg, islist, vars);
	lhs = new CompoundTerm(token, [arg]);
    }
    else if (op.fixity == "fy")
    {
	var arg = read_expression(s, op.precedence+0.5, isarg, islist, vars);
	lhs = new CompoundTerm(token, [arg]);
    }
    else
        return false; // Parse error    
    while (true)
    {
	infix_operator = peek_token(s);
	if (typeof(infix_operator) == "number" && infix_operator <= 0)
        {
            // Yuck. This is when we read something like X is A-1. Really the - is -/2 in this case
	    read_token(s);
            unread_token(s, Math.abs(infix_operator));
            unread_token(s, "-");
            infix_operator = "-";
        }
        if (infix_operator == '(')
        {
            // We are reading a term. Keep reading expressions: After each one we should
            // either get , or )
            // First though, consume the (
	    read_token(s);
            var args = [];
            var next = {};
            while (true)
            {
		var arg = read_expression(s, Infinity, true, false, vars);
		args.push(arg);
		var next = read_token(s);
		if (next == ')')
                    break;
                else if (next == ',')
                    continue;
                else
                {
                    if (next == null)
                        return syntax_error("end_of_file");
                    else
                        return syntax_error(next);
                }
            }
	    lhs = new CompoundTerm(lhs, args);
	    // Now, where were we?
	    infix_operator = peek_token(s);
	}
        // Pretend that . is an operator with infinite precedence
        if (infix_operator == ".")
        {
	    return lhs;
        }
        if (infix_operator == "," && isarg)
        {
	    return lhs;
        }
        if (infix_operator == "|" && islist)
        {
	    return lhs;
        }
        if (infix_operator == null)
        {
	    return lhs;
        }
	op = infix_operators[infix_operator];
	if (op !== undefined)
        {
            if (op.fixity == "xfx" && precedence > op.precedence)
	    {
		lhs = parse_infix(s, lhs, op.precedence, vars);
	    }
            else if (op.fixity == "xfy" && precedence > op.precedence)
            {
                // Is this 0.5 thing right? Will it eventually drive up precedence to the wrong place? We never want to reach the next integer...
		lhs = parse_infix(s, lhs, op.precedence+0.5, vars);
	    }
            else if (op.fixity == "yfx" && precedence >= op.precedence)
            {
		lhs = parse_infix(s, lhs, op.precedence, vars);
	    }
            else if (op.fixity == "xf" && precedence > op.precedence)
            {
                lhs = parse_postfix(s, lhs, op.precedence);
	    }
            else if (op.fixity == "yf" && precedence >= op.precedence)
            {
                lhs = parse_postfix(s, lhs, op.precedence);
	    }
            else
            {
		return lhs;
            }
        }
        else
        {
	    return lhs;
        }
    }
}

function peek_token(s)
{
    if (s.peek_tokens === undefined || s.peeked_tokens.length == 0 )
    {
        var tt = {};
	var tt = read_token(s);
	s.peeked_tokens = [tt];
    }
    return s.peeked_tokens[0];
}

function unread_token(s, t)
{
    if (s.peeked_tokens === undefined)
        s.peeked_tokens = [t];
    else
        s.peeked_tokens.push(t);
}

function read_token(s)
{
    if (s.peeked_tokens !== undefined && s.peeked_tokens.length != 0)
    {
	return s.peeked_tokens.pop();
    }
    return lex(s.stream);
}

function is_char(c)
{
    return ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '_');
}

var punctuation_array = ['`', '~', '@', '#', '$', '^', '&', '*', '-', '+', '=', '<', '>', '/', '?', ':', ',', '\\', '.'];

function is_punctuation(c)
{
    return punctuation_array.indexOf(c) != -1;
}

// lex(stream) returns a single token throws an exception if one is raised
function lex(s)
{
    var token;
    while(true)
    {
        var c = get_raw_char_with_conversion(s);
        if (c == -1)
        {
	    return null;
        }
        // Consume any whitespace
        if (c == ' ' || c == '\n' || c == '\t')
            continue;        
        else if (c == '%')
        {
            do
            {
                d = get_raw_char_with_conversion(s);
                if (d == -1)
                {
		    return null;
                }
            } while(d != '\n');
            continue;
        }
        else if (c == '/')
        {
            d = peek_raw_char_with_conversion(s);
            if (d == '*')
            {
                // Block comment
                get_raw_char_with_conversion(s);
                while(true)
                {
                    d = get_raw_char_with_conversion(s);
                    if (d == -1)
			throw syntax_error("end of file in block comment");
                    if (d == '*' && get_raw_char_with_conversion(s) == '/')
                        break;
                }
                continue;
            }
            else
            {
                // My mistake, the term actually begins with /. c is still set to the right thing
                break;
            }
        }
        break;
    }    
    if ((c >= 'A' && c <= 'Z') || c == '_')
    {
        token = {variable_name: "" + c};
        // Variable. May contain a-zA-Z0-9_
        while (true)
        {
            c = peek_raw_char_with_conversion(s);
            if (is_char(c))
            {
                token.variable_name += get_raw_char_with_conversion(s);
            }
            else
            {
		return token;
            }
        } 
    }
    else if ((c >= '0' && c <= '9') || (c == '-' && peek_raw_char_with_conversion(s) >= '0' && peek_raw_char_with_conversion(s) <= '9'))
    {
        // Integer. May contain 0-9 only. Floats complicate this a bit
        var negate = false;
        if (c == '-')
        {
            token = '';
            negate = true;
        }
        else
            token = c - '0';
        var decimal_places = 0;
        var seen_decimal = false;
        while (true)
        {            
            c = peek_raw_char_with_conversion(s);       
            if (seen_decimal)
                decimal_places++;
            if ((c >= '0' && c <= '9'))
            {
                token = token * 10 + (get_raw_char_with_conversion(s) - '0');
            }
            else if (c == '.' && !seen_decimal)
            {
                // Fixme: Also must check that the next char is actually a number. Otherwise 'X = 3.' will confuse things somewhat.
                seen_decimal = true;
                get_raw_char_with_conversion(s);
                continue;
            }
            else if (is_char(c))
		throw syntax_error("illegal number" + token + ": " + c);
            else
            {
                if (seen_decimal)
                {
                    for (var i = 1; i < decimal_places; i++)
                        token = token / 10;
                }
		return negate?(-token):token;
            }
        }
    }
    else 
    {
        // Either:
        // 1) a term
        // 2) an atom (which is a term with no arguments) 
        // 3) An operator
        // In all cases, first we have to read an atom
        var buffer = "";
        var state = 0;
        if (c == '\'')
        {
            // Easy. The atom is quoted!
            while(true)
            {
                c = get_raw_char_with_conversion(s);
                if (c == '\\')
                    state = (state + 1) % 2;
                if (c == -1)
		    throw syntax_error("end of file in atom");
                if (c == '\'' && state == 0)
                    break;
                buffer += c;
            }
            
        }
        else // Not so simple. Have to read an atom using rules, which are actually available only for a fee from ISO...
        {
            buffer += c;
            // An unquoted atom may contain either all punctuation or all A-Za-z0-9_. There are probably more complicated rules, but this will do
            char_atom = is_char(c);
            punctuation_atom = is_punctuation(c);
            while (true)
            {                
                c = peek_raw_char_with_conversion(s);                
                if (c == -1)
                    break;
                if (char_atom && is_char(c))
                    buffer += get_raw_char_with_conversion(s);
                else if (punctuation_atom && is_punctuation(c))
                    buffer += get_raw_char_with_conversion(s);
                else
                    break;
            }            
        }
	return buffer;
    }
}


function get_raw_char_with_conversion(s)
{
    if (!PrologFlag.values['char_conversion'])
	return Stream.get_raw_char(s);
    var t = Stream.get_raw_char(s);
    var tt = char_conversion_override[t];
    if (tt === undefined)
        return t;
    else
        return tt;
}

function peek_raw_char_with_conversion(s)
{
    if (!PrologFlag.values['char_conversion'])
	return Stream.peek_raw_char(s);
    var t = Stream.peek_raw_char(s);
    var tt = char_conversion_override[t];
    if (tt === undefined)
        return t;
    else
        return tt;
}

function make_list(items, tail)
{
    var result = tail;
    for (i = items.length-1; i >= 0; i--)
	result = new CompoundTerm(Constants.listFunctor, [items[i], result]);
    return result;
}

function readTerm(stream, options)
{
    return read_expression({stream:stream, peeked_token: undefined}, Infinity, false, false, {});
}

function parser_test_read(stream, size, count, buffer)
{
    var bytes_read = 0;
    var records_read;
    for (records_read = 0; records_read < count; records_read++)
    {
        for (var b = 0; b < size; b++)
        {
            t = stream.data.shift();
            if (t === undefined)
            {                
                return records_read;
            }
            buffer[bytes_read++] = t;
        }
    }
    return records_read;
}

function doTest(atom)
{
    var s = {peeked_token: undefined,
	     stream: Stream.new_stream(parser_test_read,
				       null,
				       null,
				       null,
				       null,
				       IO.toByteArray(atom))};
    return read_expression(s, Infinity, false, false, {});
}

module.exports = {readTerm: readTerm,
		  test: doTest};
