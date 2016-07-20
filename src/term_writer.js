"use strict";
exports=module.exports;

var AtomTerm = require('./atom_term');
var VariableTerm = require('./variable_term');
var FloatTerm = require('./float_term');
var CompoundTerm = require('./compound_term');
var IntegerTerm = require('./integer_term');
var Parser = require('./parser');
var Constants = require('./constants');
var Operators = require('./operators');
var Stream = require('./stream');

function needsQuote(s)
{
    if (s.length == 0)
        return true;
    var ch = s.charAt(0);
    if (ch == '%')         // Things that look like comments need to be quoted
        return true;
    if (solo.indexOf(ch) != -1)
        return s.length != 1;
    if (Parser.is_graphic_char(ch))
    {
        // Things that look like comments need to be quoted
        if (s.length >= 2 && s.substring(0, 2) == '/*')
            return true;
        // If all characters are graphic, then we are OK. Otherwise we must quote
        for (var i = 1; i < s.length; i++)
        {
            if (!Parser.is_graphic_char(s.charAt(i)))
                return true;
        }
        return false;
    }
    else if (isAtomStart(ch))
    {
        // If there are only atom_char characters then we are OK. Otherwise we must quote
        for (var i = 1; i < s.length; i++)
        {
            if (!isAtomContinuation(ch))
                return true;
        }
        return false;
    }
    return true;
}

var solo = [';','!','(',')',',','[',']','{','}','%'];
function isAtomStart(ch)
{
    // FIXME: And also anything that unicode considers lower-case, I guess?
    return ch >= 'a' && ch <= 'z';
}

function isAtomContinuation(ch)
{
    // FIXME: And also anything that unicode considers to be not punctuation? I have no idea.
    return (ch >= 'a' && ch <= 'z')
        || (ch >= 'A' && ch <= 'Z')
        || (ch >= '0' && ch <= '9')
        || (ch == '_');
}

function formatAtom(options, term)
{
    if (options.quoted)
    {
        if (term.equals(Constants.emptyListAtom))
            return term.value;
        if (needsQuote(term.value))
            return quoteString(term.value);
        return term.value;
    }
    else
        return term.value;
}

function quoteString(s)
{
    var out = "'";
    for (var i = 0; i < s.length; i++)
    {
        var ch = s.charAt(i);
        if (ch == "'")
            out += "\\'";
        else if (ch == "\\")
            out += "\\\\";
        else if (ch == '\u0007')
            out += "\\a";
        else if (ch == '\u0008')
            out += "\\b";
        else if (ch == '\u000C')
            out += "\\f";
        else if (ch == '\u000A')
            out += "\\n";
        else if (ch == '\u0009')
            out += "\\t";
        else if (ch == '\u000B')
            out += "\\v";
        else if (ch == '\u000D')
            out += "\\r";
        else
            out += ch;
    }
    return out + "'";
}

function getOp(functor, options)
{
    if (options.operators !== undefined)
        return options.operators[functor.toString()];
    if (functor.arity < 1 || functor.arity > 2)
        return null;
    var op = Operators[functor.name.value];
    if (op != undefined)
    {
        if (functor.arity == 1)
            return op.prefix;
        else (functor.arity == 2)
            return op.infix;
    }
    return null;
}

// The reason that we generate lots and lots of strings here is that it's not possible to deterministically determine
// how to print a term without lookahead. Consider is(a,b) and +(a,b). These should be printed as "a is b" and "a+b" respectively
// but until we have determined the first character of the second argument we cannot determine if there should be a space
// We cannot just look directly ahead either, since the right hand side may involve a term with a prefix-operator like +(a,-b)
// It might be possible to solve this more efficiently, but for now, just convert everything into strings and then assemble it
// later into one big string, then print that to the stream just once
function glue_atoms(a, b)
{
    if (Parser.is_graphic_char(a.charAt(a.length-1)) && !Parser.is_graphic_char(b.charAt(0)))
        return a+b;
    else if (!Parser.is_graphic_char(a.charAt(a.length-1)) && Parser.is_graphic_char(b.charAt(0)))
        return a+b;
    else
        return a+" "+b;
}

function formatTerm(options, precedence, term)
{
    //console.log("term: " + term);
    if (term === undefined)
        throw new Error("Undefined term passed to formatTerm");
    term = term.dereference();
    if (term.formatTerm !== undefined)
    {
        return term.formatTerm(options, precedence);
    }
    if (term instanceof VariableTerm)
    {
        return "_" + term.index;
    }
    else if (term instanceof IntegerTerm)
    {
        return String(term.value);
    }
    else if (term instanceof FloatTerm)
    {
        // CHECKME: If .quoted == true then we have to make sure this can be read in again. Consider things like 3e-10
        return String(term.value);
    }
    else if (term instanceof AtomTerm)
    {
        return formatAtom(options, term);
    }
    else if (term instanceof CompoundTerm && term.functor.equals(Constants.numberedVarFunctor) && term.args[0] instanceof IntegerTerm && options.numbervars)
    {
        var i = term.args[0].value % 26;
        var j = Math.trunc(term.args[0].value / 26);
        if (j == 0)
            return String.fromCharCode(65+i);
        else
            return String.fromCharCode(65+i) + String(j);
    }
    else if (term instanceof CompoundTerm)
    {
        // if arg is a current operator
        var op = getOp(term.functor, options);
        if (term.functor.equals(Constants.listFunctor) && options.ignore_ops != true)
        {
            var output = '[';
            var head = term.args[0];
            var tail = term.args[1].dereference();
            while (true)
            {
                output += formatTerm(options, 1200, head);
                if (tail instanceof CompoundTerm && tail.functor.equals(Constants.listFunctor))
                {
                    output += ',';
                    head = tail.args[0];
                    tail = tail.args[1].dereference();
                    continue;
                }
                else if (tail.equals(Constants.emptyListAtom))
                    return output + "]";
                else
                    return output + "|" + formatTerm(options, 1200, tail) + "]";
            }
        }
        else if (term.functor.equals(Constants.curlyFunctor) && options.ignore_ops != true)
        {
            var output = '{';
            var head = term.args[0];
            while (head instanceof CompoundTerm && head.functor.equals(Constants.conjunctionFunctor))
            {
                output += formatTerm(options, 1200, head.args[0]) + ',';
                head = head.args[1];
            }
            return output + formatTerm(options, 1200, head) + '}'
        }
        else if (op == null || options.ignore_ops == true)
        {
            var output = formatAtom(options, term.functor.name) + '(';
            for (var i = 0; i < term.args.length; i++)
            {
                output += formatTerm(options, 1200, term.args[i]);
                if (i+1 < term.args.length)
                    output += ',';
            }
            return output + ')';
        }
        else if (op != null && options.ignore_ops != true)
        {
            var output = '';
            if (op.precedence > precedence)
                output = '(';
            switch (op.fixity)
            {
                case "fx":
                {
                    output = glue_atoms(output+formatAtom(options, term.functor.name), formatTerm(options, precedence-1, term.args[0]));
                    break;
                }
                case "fy":
                {
                    output = glue_atoms(output+formatAtom(options, term.functor.name), formatTerm(options, precedence, term.args[0]));
                    break;
                }
                case "xfx":
                {
                    output = glue_atoms(output+formatTerm(options, precedence-1, term.args[0]), formatAtom(options, term.functor.name));
                    output = glue_atoms(output,formatTerm(options, precedence-1, term.args[1]));
                    break;
                }
                case "xfy":
                {
                    output = glue_atoms(output+formatTerm(options, precedence-1, term.args[0]), formatAtom(options, term.functor.name));
                    output = glue_atoms(output,formatTerm(options, precedence, term.args[1]));
                    break;
                }
                case "yfx":
                {
                    output = glue_atoms(output+formatTerm(options, precedence, term.args[0]), formatAtom(options, term.functor.name));
                    output = glue_atoms(output,formatTerm(options, precedence-1, term.args[1]));
                    break;
                }
                case "xf":
                {
                    output = glue_atoms(output+formatTerm(options, precedence-1, term.args[0]), formatAtom(options, term.functor.name));
                    break;
                }
                case "yf":
                {
                    output = glue_atoms(output+formatTerm(options, precedence, term.args[0]), formatAtom(options, term.functor.name));
                    break;
                }
                default:
                {
                    throw new Error("Illegal op fixity: " + op.fixity);
                }
            }
            if (op.precedence > precedence)
                return output + ')';
            return output;
        }
        else
        {
            throw new Error("Unable to print compound term");
        }
    }
    else
    {
        throw new Error("Unable to print term:" + util.inspect(term));
    }
}

module.exports.writeTerm = function(stream, term, options)
{
    var text = formatTerm(options, 1200, term);
    var bufferObject = Stream.stringBuffer(text.toString());
    return stream.write(stream, 0, bufferObject.buffer.length, bufferObject.buffer) >= 0;
}

module.exports.formatTerm = formatTerm;
