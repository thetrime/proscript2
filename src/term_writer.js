var AtomTerm = require('./atom_term');
var VariableTerm = require('./variable_term');
var FloatTerm = require('./float_term');
var CompoundTerm = require('./compound_term');
var IntegerTerm = require('./integer_term');
var Functor = require('./functor');
var Parser = require('./parser');
var ArrayUtils = require('./array_utils');
var Constants = require('./constants');

function outputString(stream, value)
{
    var bytes = ArrayUtils.toByteArray(value.toString());
    return stream.write(stream, bytes.length, bytes) >= 0;
}

function outputAtom(stream, options, term)
{
    if (options.quoted)
    {
        if (needsQuote(term.value))
            outputString(stream, quoteString(term.value));
        else
            outputString(stream, term.value);
    }
    else
        outputString(stream, term.value);
}

function getOp(functor, options)
{
    if (options.operators !== undefined)
        return options.operators[functor.toString()];
    for (var i = 0; i < Parser.standard_operators; i++)
    {
        if (Parser.standard_operators[i].functor.equals(functor))
            return Parser.standard_operators[i];
    }
    return null;
}

function outputTerm(stream, options, precedence, term)
{
    if (term === undefined)
        throw new Error("Undefined term passed to writeTerm");
    term = term.dereference();
    if (term instanceof VariableTerm)
    {
        outputString(stream, "_" + term.index);
    }
    else if (term instanceof IntegerTerm)
    {
        outputString(stream, String(term.value));
    }
    else if (term instanceof FloatTerm)
    {
        // CHECKME: If .quoted == true then we have to make sure this can be read in again. Consider things like 3e-10
        outputString(stream, String(term.value));
    }
    else if (term instanceof AtomTerm)
    {
        outputAtom(stream, options, term);
    }
    else if (term instanceof CompoundTerm && term.functor.equals(Constants.numberedVarFunctor) && term.args[0] instanceof IntegerTerm && options.numbervars)
    {
        var i = term.args[0].value % 26;
        var j = Math.trunc(term.args[0].value / 26);
        if (j == 0)
            outputString(stream, String.fromCharCode(65+i));
        else
            outputString(stream, String.fromCharCode(65+i) + String(j));
    }
    else if (term instanceof CompoundTerm)
    {
        // if arg is a current operator
        var op = getOp(term.functor, options);
        if (term.functor.equals(Constants.listFunctor) && options.ignore_ops != true)
        {
            outputString(stream, '[');
            var head = term.args[0];
            var tail = term.args[1];
            while (true)
            {
                outputTerm(stream, options, 1200, head);
                if (tail instanceof CompoundTerm && tail.functor.equals(Constants.listFunctor))
                {
                    outputString(stream, ',');
                    head = tail.args[0];
                    tail = tail.args[1];
                    continue;
                }
                else if (tail.equals(Constants.emptyListAtom))
                {
                    outputString(stream, ']');
                    break;
                }
                else
                {
                    outputString(stream, '|');
                    outputTerm(stream, options, 1200, tail);
                    outputString(stream, ']');
                    break;
                }
            }
        }
        else if (term.functor.equals(Constants.curlyFunctor) && options.ignore_ops != true)
        {
            outputString(stream, '{');
            var head = term.args[0];
            while (head instanceof CompoundTerm && head.functor.equals(Constants.conjunctionFunctor))
            {
                outputTerm(stream, options, 1200, head.args[0]);
                outputString(stream, ',');
                head = head.args[1];
            }
            outputTerm(stream, options, 1200, head);
            outputString(stream, '}');
        }
        else if (op == null || options.ignore_ops == true)
        {
            outputAtom(stream, options, term.functor.name);
            outputString(stream, '(');
            for (var i = 0; i < term.args.length; i++)
            {
                outputTerm(stream, options, 1200, term.args[i]);
                if (i+1 < term.args.length)
                    outputString(stream, ',');
            }
            outputString(stream, ')');
        }
        else if (op != null && options.ignore_ops != true)
        {
            if (op.precedence > precedence)
                outputString(stream, '(');
            switch (op.fixity)
            {
                case "fx":
                {
                    outputAtom(stream, options, term.functor.name);
                    outputString(stream, ' '); // FIXME: Maybe. Depends on interaction between term and op :(
                    outputTerm(stream, options, precedence-1, term.args[0]);
                    break;
                }
                case "fy":
                {
                    outputAtom(stream, options, term.functor.name);
                    outputString(stream, ' ');
                    outputTerm(stream, options, precedence, term.args[0]);
                    break;
                }
                case "xfx":
                {
                    outputTerm(stream, options, precedence-1, term.args[0]);
                    outputString(stream, ' ');
                    outputAtom(stream, options, term.functor.name);
                    outputString(stream, ' ');
                    outputTerm(stream, options, precedence-1, term.args[1]);
                    break;
                }
                case "xfy":
                {
                    outputTerm(stream, options, precedence-1, term.args[0]);
                    outputString(stream, ' ');
                    outputAtom(stream, options, term.functor.name);
                    outputString(stream, ' ');
                    outputTerm(stream, options, precedence, term.args[1]);
                    break;
                }
                case "yfx":
                {
                    outputTerm(stream, options, precedence, term.args[0]);
                    outputString(stream, ' ');
                    outputAtom(stream, options, term.functor.name);
                    outputString(stream, ' ');
                    outputTerm(stream, options, precedence-1, term.args[1]);
                    break;
                }
                case "xf":
                {
                    outputTerm(stream, options, precedence-1, term.args[0]);
                    outputString(stream, ' ');
                    outputAtom(stream, options, term.functor.name);
                    break;
                }
                case "yf":
                {
                    outputTerm(stream, options, precedence, term.args[0]);
                    outputString(stream, ' ');
                    outputAtom(stream, options, term.functor.name);
                    break;
                }
                default:
                {
                    throw new Error("Illegal op fixity: " + op.fixity);
                }
            }
            if (op.precedence > precedence)
                outputString(stream, ')');
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
    outputTerm(stream, options, 1200, term, []);
}
