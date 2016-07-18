/* Term reading */
// See http://journal.stuffwithstuff.com/2011/03/19/pratt-parsers-expression-parsing-made-easy/

var Stream = require('./stream.js');
var PrologFlag = require('./prolog_flag.js');
var CompoundTerm = require('./compound_term.js');
var VariableTerm = require('./variable_term.js');
var IntegerTerm = require('./integer_term.js');
var AtomTerm = require('./atom_term.js');
var BigInteger = require('big-integer');
var NumericTerm = require('./numeric_term.js');
var Functor = require('./functor.js');
var FloatTerm = require('./float_term.js');
var Constants = require('./constants.js');
var Errors = require('./errors.js');
var Operators = require('./operators.js');
var Utils = require('./utils.js');
var CharConversionTable = require('./char_conversion.js');
var util = require('util');

function ExplicitFloat(s)
{
    this.value = parseFloat(s);
}

function PrologString(s)
{
    this.value = s;
}


var ListOpenToken = {toString: function() {return "<list-open>";}};
var ListCloseToken = {toString: function() {return "<list-close>";}};
var ParenOpenToken = {toString: function() {return "<paren-open>";}};
var ParenCloseToken = {toString: function() {return "<paren-close>";}};

var CurlyOpenToken = {toString: function() {return "<curly-open>";}};
var CurlyCloseToken = {toString: function() {return "<curly-close>";}};
var DoubleQuoteToken = {toString: function() {return "<double-quote>";}};
var BarToken = {toString: function() {return "<bar>";}};



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
    Errors.syntaxError(new AtomTerm(util.inspect(msg)));
}

// this should be called with token assigned either an integer or a float
function numberToken(token)
{
    if (!isNaN(token))
    {
        if (parseInt(token) == token)
            return NumericTerm.get(token);
        if (parseInt(token) == parseFloat(token)) // very large integer
            return NumericTerm.get(token);
        if (parseFloat(Number(token)) == token)
            return new FloatTerm(parseFloat(token));
    }
    else if (token instanceof BigInteger)
        return new BigIntegerTerm(token);
    else if (token instanceof ExplicitFloat)
        return new FloatTerm(token.value);
    return null;
}

function atomicToken(token)
{
    //console.log("Token: " + token);
    if (typeof(token) === 'number' || token instanceof BigInteger || token instanceof ExplicitFloat)
    {
        var number = numberToken(token);
        if (number != null)
        {
            return number;
        }
    }
    if (token instanceof PrologString)
    {
        if (PrologFlag.values.double_quotes == "codes")
        {
            var list = [];
            for (var i = 0; i < token.value.length; i++)
                list.push(new IntegerTerm(token.value.charCodeAt(i)));
            return Utils.from_list(list);
        }
        else if (PrologFlag.values.double_quotes == "chars")
        {
            var list = [];
            for (var i = 0; i < token.value.length; i++)
                list.push(new AtomTerm(token.value.charAt(i)));
            return Utils.from_list(list);
        }
        else if (PrologFlag.values.double_quotes == "atom")
        {
            return new AtomTerm(token.value);
        }
    }
    return new AtomTerm(token);
}

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
    if (token == ListCloseToken) // end of list
        return token;
    var lhs;
    // Either the token is an operator, or it must be an atom (or the start of a list or curly-list)
    var op = (Operators[token] || {}).prefix;
    // There are some caveats here. For example, when reading [-] or foo(is, is) the thing is not actually an operator.
    var peeked_token = peek_token(s);
    if (peeked_token == ListCloseToken || peeked_token == ParenCloseToken || peeked_token == ',') // Maybe others? This is a bit ugly :(
        op = undefined;
    if (peeked_token == ParenOpenToken)
    {
        var q = read_token(s);
        var pp = peek_token(s);
        unread_token(s, ParenOpenToken);
        if (pp != ParenOpenToken)
        {
            // for example -(a,b) is not -/1 but -/2. To have -( and still get -/1 we need to see -(( I think
            op = undefined;
        }
    }
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
        else if (token == ListOpenToken || token == CurlyOpenToken)
        {
            // The principle for both of these conis very similar
            var args = [];
            var next = {};
            while(true)
            {
                var t = read_expression(s, Infinity, true, true, vars);
                if (t == ListCloseToken && token == ListOpenToken)
                {
                    lhs = Constants.emptyListAtom;
                    break;
                    // Special case for the empty list, since the first argument is ']'
                }
                if (t == CurlyCloseToken && token == CurlyOpenToken)
                {
                    // Special case for the empty curly which is an atom
                    lhs = Constants.curlyAtom;
                    break;
                }
                args.push(t);
                var next = read_token(s);
                if (next == ',')
                    continue;
                else if (next == ListCloseToken && token == ListOpenToken)
		{
                    lhs = make_list(args, Constants.emptyListAtom);
		    break;
                }
                else if (next == CurlyCloseToken && token == CurlyOpenToken)
                {
                    lhs = make_curly(args);
                    break;
                }
                else if (next == BarToken && token == ListOpenToken)
                {
		    var tail = read_expression(s, Infinity, true, true, vars);
		    lhs = make_list(args, tail);
		    var next = read_token(s);
                    if (next == ListCloseToken)
                        break;
                    else
                        return syntax_error("missing ]");
                }
                else
                    return syntax_error("mismatched " + token + " at " + next);
            }
        }
        else if (token == ParenOpenToken)
        {
            // Is this right? () just increases the precedence to infinity and reads another term?
            var lhs = read_expression(s, Infinity, false, false, vars)
            var next = read_token(s);
            if (next != ParenCloseToken)
                return syntax_error("mismatched ( at " + next);
	}
	else if (token.variable_name != undefined)
	{
            // It is a variable
            if (token.variable_name[0] == "_")
                lhs = new VariableTerm("_");
            else
            {
                if (vars[token.variable_name] === undefined)
                    vars[token.variable_name] = new VariableTerm(token.variable_name);
                lhs = vars[token.variable_name];
            }
        }
        else if (token == CurlyCloseToken)
        {
            lhs = token;
        }
        else
        {
            // It is an atomic term
            lhs = atomicToken(token);
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
        if (infix_operator == ParenOpenToken)
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
                if (next == ParenCloseToken)
                    break;
                else if (next == ',')
                    continue;
                else
                {
                    if (next == null)
                        return syntax_error("end_of_file");
                    else
                        return syntax_error("Unexpected: " + util.inspect(next));
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
        if (infix_operator == BarToken && islist)
        {
	    return lhs;
        }
        if (infix_operator == null)
        {
	    return lhs;
        }
        op = (Operators[infix_operator] || {}).infix;
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
    if (s.peeked_tokens === undefined || s.peeked_tokens.length == 0 )
    {
        var tt = {};
	var tt = read_token(s);
	s.peeked_tokens = [tt];
    }
    return s.peeked_tokens[s.peeked_tokens.length-1];
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
    var q = lex(s.stream);
    //console.log("lexed: " + q);
    return q;
}

function is_char(c)
{
    return ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '_');
}

//var graphic_array = ['`', '~', '@', '#', '$', '^', '&', '*', '-', '+', '=', '<', '>', '/', '?', ':', ',', '\\', '.'];
var graphic_array = ['#', '$', '&', '*', '+', '-', '.', '/', ':', '<', '=', '>', '?', '@', '^', '~'];

function is_graphic_char(c)
{
    return graphic_array.indexOf(c) != -1 || c == '\\'; // graphic-token-char is either graphic-char or backslash-char
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
    if (c == "[")
    {
        if (peek_raw_char_with_conversion(s) == "]")
        {
            get_raw_char_with_conversion(s);
            return "[]";
        }
        return ListOpenToken;
    }
    if (c == "(")
        return ParenOpenToken;
    if (c == ")")
        return ParenCloseToken;
    if (c == "{")
        return CurlyOpenToken;
    else if (c == "}")
        return CurlyCloseToken;
    else if (c == "]")
        return ListCloseToken;
    else if (c == "|")
        return BarToken;

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
        if (c == "0" && peek_raw_char_with_conversion(s) == "'")
        {
            // Char code
            get_raw_char_with_conversion(s);
            return get_raw_char_with_conversion(s).charCodeAt(0)
        }
        // FIXME: Also need to handle 0x and 0b here

        // Parse a number
        var token = c;
        var seen_decimal = false;
        while(true)
        {
            c = peek_raw_char_with_conversion(s);
            if (c >= '0' && c <= '9')
            {
                token = token + c;
                get_raw_char_with_conversion(s);
            }
            else if (c == '.' && !seen_decimal)
            {
                token += '.';
                get_raw_char_with_conversion(s);
                var p = peek_raw_char_with_conversion(s);
                if (p >= '0' && p <= '9')
                    seen_decimal = true;
                else
                {
                    // This is like "X = 3."
                    // We have to unget the '.' here; the parser keeps one token as in a buffer called lookahead
                    // get/peek_raw_char_with_conversion will return this (converted) char if we set it
                    unget_raw_char('.');
                    break;
                }
            }
            else if (is_char(c))
                throw syntax_error("illegal number" + token + ": " + c);
            else
                break;
        }
        if (!seen_decimal && token.length < 16)
        {
            return parseInt(token);
        }
        if (seen_decimal)
        {
            if (parseInt(token) == parseFloat(token))
            {
                // care must be taken here. We do not want 1.0 to be turned into an IntegerTerm
                return new ExplicitFloat(token);
            }
            return parseFloat(token);
        }
        if (parseInt(token) == parseFloat(token)) // FIXME: only if unbounded! Otherwise float
        {
            return new BigInteger(token);
        }
        return parseInt(token); // just barely fits
    }
    else 
    {
        // Either:
        // 1) a term
        // 2) an atom (which is a term with no arguments) 
        // 3) An operator
        // In all cases, first we have to read an atom
        var buffer = "";
        var is_escape = false;
        if (c == "'" || c == '"')
        {
            var matcher = c;
            // Easy. The atom is quoted!
            while(true)
            {
                c = get_raw_char_with_conversion(s);
                if (c == -1)
                    throw syntax_error("end of file in atom");
                if (c == '\\')
                {
                    if (is_escape)
                        buffer += "\\";
                    is_escape = !is_escape;
                    continue;
                }
                else if (is_escape)
                {
                    // escape/control character like \n
                    if (c == "x")
                    {
                        // hex code
                        buffer += String.fromCharCode(parseInt(get_raw_char_with_conversion(s) + get_raw_char_with_conversion(s), 16));
                        if (peek_raw_char_with_conversion(s) == "\\")
                            get_raw_char_with_conversion(s);
                    }
                    else if (c == "n")
                        buffer += "\n";
                    else if (c == "t")
                        buffer += "\t";
                    else if (c == "'")
                        buffer += "'";
                    else if (c == "u")
                    {
                        // unicode point
                        buffer += String.fromCharCode(parseInt(get_raw_char_with_conversion(s) + get_raw_char_with_conversion(s)+get_raw_char_with_conversion(s) + get_raw_char_with_conversion(s), 16));
                        if (peek_raw_char_with_conversion(s) == "\\")
                            get_raw_char_with_conversion(s);

                    }
                    else
                    {
                        console.log("unexpected escape code " + c);
                    }
                    is_escape = false;
                    continue;
                }
                if (c == matcher && !is_escape)
                {
                    // This happens if we read something like '''' which is a valid encoding for a single quote
                    if (peek_raw_char_with_conversion(s) == matcher)
                    {
                        get_raw_char_with_conversion(s);
                    }
                    else
                        break;
                }
                buffer += c;
            }
            if (matcher == '"')
                return new PrologString(buffer);
        }
        else // Not so simple. Have to read an atom using rules, which are actually available only for a fee from ISO...
        {
            buffer += c;
            // An unquoted atom may contain either all punctuation or all A-Za-z0-9_. There are probably more complicated rules, but this will do
            char_atom = is_char(c);
            punctuation_atom = is_graphic_char(c);
            while (true)
            {
                c = peek_raw_char_with_conversion(s);                
                if (c == -1)
                    break;
                if (char_atom && is_char(c))
                    buffer += get_raw_char_with_conversion(s);
                else if (punctuation_atom && is_graphic_char(c))
                    buffer += get_raw_char_with_conversion(s);
                else
                    break;
            }
        }
	return buffer;
    }
}

var lookahead = null;

function unget_raw_char(c)
{
    lookahead = c;
}

function get_raw_char_with_conversion(s)
{
    if (lookahead != null)
    {
        var c = lookahead;
        lookahead = null;
        return c;
    }
    if (!PrologFlag.values['char_conversion'])
        return s.get_raw_char();
    var t = s.get_raw_char();
    var tt = CharConversionTable[t];
    if (tt === undefined)
        return t;
    else
        return tt;
}

function peek_raw_char_with_conversion(s)
{
    if (lookahead != null)
    {
        return lookahead;
    }
    if (!PrologFlag.values['char_conversion'])
        return s.peek_raw_char();
    var t = s.peek_raw_char();
    var tt = CharConversionTable[t];
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

function make_curly(items)
{
    var result = items[items.length-1];
    for (i = items.length-2; i >= 0; i--)
        result = new CompoundTerm(Constants.conjunctionFunctor, [items[i], result]);
    return new CompoundTerm(Constants.curlyFunctor, [result]);
}



function readTerm(stream, options)
{
    var v = read_expression({stream:stream, peeked_token: undefined}, Infinity, false, false, {});
    if (v == null)
        return Constants.endOfFileAtom;
    return v;
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

function tokenToNumericTerm(token)
{
    // trim whitespace from the start of the token. Apparently that IS allowed
    token = token.replace(/^\s+/gm,'');
    if (token.indexOf('.') != -1)
        return new FloatTerm(parseFloat(token));
    if (token.indexOf('E') != -1)
        return new FloatTerm(parseFloat(token));
    if (token.substring(0, 2) == "0'" && token.length == 3)
    {
        return new IntegerTerm(token.charCodeAt(2));
    }
    if (token.substring(0, 2) == "0x")
    {
        token = token.substring(2);
        if (parseInt(token, 16).toString(16) == token)
            return new IntegerTerm(parseInt(token, 16));
        // FIXME: biginteger hex?
        Errors.syntaxError(new AtomTerm("illegal number " + token))
    }
    if (token.substring(0, 2) == "0b")
    {
        token = token.substring(2);
        if (parseInt(token, 2).toString(2) == token)
            return new IntegerTerm(parseInt(token, 2));
        // FIXME: biginteger binary?
        Errors.syntaxError(new AtomTerm("illegal number " + token))
    }
    if (token.substring(0, 2) == "0o")
    {
        token = token.substring(2);
        if (parseInt(token, 8).toString(8) == token)
            return new IntegerTerm(parseInt(token, 8));
        // FIXME: biginteger octal?
        Errors.syntaxError(new AtomTerm("illegal number " + token))
    }
    if (token.length < 16)
    {
        // Javascript is VERY lenient about parsing numbers. Prolog should not be.
        // If converting the token to a number then printing it back to a string is not the same
        // as the original token, then it is NOT a simple integer
        if (parseInt(token).toString() == token)
            return new IntegerTerm(parseInt(token));
        Errors.syntaxError(new AtomTerm("illegal number " + token))
    }
    if (parseFloat(token) == parseInt(token))
        return new BigIntegerTerm(new BigInteger(token));
    return new IntegerTerm(parseInt(token));

}

module.exports = {readTerm: readTerm,
                  numberToken: numberToken,
                  atomicToken: atomicToken,
                  tokenToNumericTerm: tokenToNumericTerm,
                  is_graphic_char:is_graphic_char};
