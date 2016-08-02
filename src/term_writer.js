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
var CTable = require('./ctable');

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
            if (!isAtomContinuation(s.charAt(i)))
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
    var op = Operators[CTable.get(functor.name).value];
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
    term = DEREF(term);
    if (TAGOF(term) == VariableTag)
    {
        return "_G" + term;
    }
    else if (TAGOF(term) == ConstantTag)
    {
        term = CTable.get(term);
        if (term.formatTerm !== undefined)
        {
            return term.formatTerm(options, precedence);
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
    }
    else if (options.numbervars && (TAGOF(term) == CompoundTag) && (FUNCTOROF(term) == Constants.numberedVarFunctor) && (TAGOF(ARGOF(term, 0)) == ConstantTag) && CTable.get(term) instanceof IntegerTerm)
    {
        term = CTable.get(ARGOF(term, 0));
        var i = term.value % 26;
        var j = Math.trunc(term.value / 26);
        if (j == 0)
            return String.fromCharCode(65+i);
        else
            return String.fromCharCode(65+i) + String(j);
    }
    else if (TAGOF(term) == CompoundTag)
    {
        // if arg is a current operator
        var functor = FUNCTOROF(term);
        var op = getOp(CTable.get(functor), options);
        if (functor == Constants.listFunctor && options.ignore_ops != true)
        {
            var output = '[';
            var head = ARGOF(term, 0);
            var tail = ARGOF(term, 1);
            while (true)
            {
                output += formatTerm(options, 1200, head);
                if (TAGOF(tail) == CompoundTag && FUNCTOROF(tail) == Constants.listFunctor)
                {
                    output += ',';
                    head = ARGOF(tail, 0);
                    tail = ARGOF(tail, 1);
                    continue;
                }
                else if (tail == Constants.emptyListAtom)
                    return output + "]";
                else
                    return output + "|" + formatTerm(options, 1200, tail) + "]";
            }
        }
        else if (functor == Constants.curlyFunctor && options.ignore_ops != true)
        {
            var output = '{';
            var head = ARGOF(term, 0);
            while (TAGOF(head) == CompoundTag && FUNCTOROF(head) == Constants.conjunctionFunctor)
            {
                output += formatTerm(options, 1200, ARGOF(head, 0)) + ',';
                head = ARGOF(head, 1);
            }
            return output + formatTerm(options, 1200, head) + '}'
        }
        else if (op == null || options.ignore_ops == true)
        {
            var functor_object = CTable.get(FUNCTOROF(term));
            var output = formatAtom(options, CTable.get(functor_object.name)) + '(';
            for (var i = 0; i < functor_object.arity; i++)
            {
                output += formatTerm(options, 1200, ARGOF(term, i));
                if (i+1 < functor_object.arity)
                    output += ',';
            }
            return output + ')';
        }
        else if (op != null && options.ignore_ops != true)
        {
            var output = '';
            if (op.precedence > precedence)
                output = '(';
            var functor_object = CTable.get(FUNCTOROF(term));
            switch (op.fixity)
            {
                case "fx":
                {
                    output = glue_atoms(output+formatAtom(options, CTable.get(functor_object.name)), formatTerm(options, precedence-1, ARGOF(term, 0)));
                    break;
                }
                case "fy":
                {
                    output = glue_atoms(output+formatAtom(options, CTable.get(functor_object.name)), formatTerm(options, precedence, ARGOF(term, 0)));
                    break;
                }
                case "xfx":
                {
                    output = glue_atoms(output+formatTerm(options, precedence-1, ARGOF(term, 0)), formatAtom(options, CTable.get(functor_object.name)));
                    output = glue_atoms(output,formatTerm(options, precedence-1, ARGOF(term, 1)));
                    break;
                }
                case "xfy":
                {
                    output = glue_atoms(output+formatTerm(options, precedence-1, ARGOF(term, 0)), formatAtom(options, CTable.get(functor_object.name)));
                    output = glue_atoms(output,formatTerm(options, precedence, ARGOF(term, 1)));
                    break;
                }
                case "yfx":
                {
                    output = glue_atoms(output+formatTerm(options, precedence, ARGOF(term, 0)), formatAtom(options, CTable.get(functor_object.name)));
                    output = glue_atoms(output,formatTerm(options, precedence-1, ARGOF(term, 1)));
                    break;
                }
                case "xf":
                {
                    output = glue_atoms(output+formatTerm(options, precedence-1, ARGOF(term, 0)), formatAtom(options, CTable.get(functor_object.name)));
                    break;
                }
                case "yf":
                {
                    output = glue_atoms(output+formatTerm(options, precedence, ARGOF(term, 0)), formatAtom(options, CTable.get(functor_object.name)));
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
