"use strict";
exports=module.exports;

// This module contains non-ISO extensions
var Constants = require('./constants');
var Stream = require('./stream');
var IntegerTerm = require('./integer_term');
var AtomTerm = require('./atom_term');
var FloatTerm = require('./float_term');
var BigIntegerTerm = require('./float_term');
var RationalTerm = require('./rational_term');
var Functor = require('./functor');
var CompoundTerm = require('./compound_term');
var Utils = require('./utils');
var util = require('util');
var Errors = require('./errors');
var TermWriter = require('./term_writer');
var Parser = require('./parser');
var Arithmetic = require('./arithmetic');

module.exports.term_variables = function(t, vt)
{
    // FIXME: term_variables is not defined!
    return this.unify(Utils.term_from_list(term_variables(t), Constants.emptyListAtom), vt);
}

module.exports["$det"] = function()
{
    // Since $det is an actual predicate that gets called, we want to look at the /parent/ frame to see if THAT was deterministic
    // Obviously $det/0 itself is always deterministic!
    console.log("Parent frame " + this.currentFrame.parent.functor + " has choicepoint of " + this.currentFrame.parent.choicepoint + ", there are " + this.choicepoints.length + " active choicepoints");
    return this.currentFrame.parent.choicepoint == this.choicepoints.length;
}

module.exports["$choicepoint_depth"] = function(t)
{
    return this.unify(t, new IntegerTerm(this.choicepoints.length));
}

module.exports.keysort = function(unsorted, sorted)
{
    var list = Utils.to_list(unsorted);

    for (var i = 0; i < list.length; i++)
    {
        if (!(list[i] instanceof CompoundTerm && list[i].functor.equals(Constants.pairFunctor)))
            Errors.typeError(Constants.pairAtom, list[i]);
        list[i] = {position: i, value: list[i]};
    }
    list.sort(function(a,b)
              {
                  var d = Utils.difference(a.value.args[0], b.value.args[0]);
                  // Ensure that the sort is stable
                  if (d == 0)
                      return a.position - b.position;
                  return d;
              });
    var result = Constants.emptyListAtom;
    for (var i = list.length-1; i >= 0; i--)
        result = new CompoundTerm(Constants.listFunctor, [list[i].value, result]);
    return this.unify(sorted, result);
}

module.exports.sort = function(unsorted, sorted)
{
    var list = Utils.to_list(unsorted);
    list.sort(Utils.difference);
    var last = null;
    var result = Constants.emptyListAtom;
    // remove duplicates as we go. Remember the last thing we saw in variable 'last', then only add the current item to the output if it is different
    for (var i = list.length-1; i >= 0; i--)
    {
        if (last == null || !(list[i].equals(last)))
        {
            last = list[i].dereference();
            result = new CompoundTerm(Constants.listFunctor, [last, result]);
        }
    }
    return this.unify(sorted, result);
}

module.exports.is_list = function(t)
{
    while (t instanceof CompoundTerm)
    {
        if (t.functor.equals(Constants.listFunctor))
            t = t.args[1].dereference();
        else
            return false;
    }
    return Constants.emptyListAtom.equals(t);
}

module.exports.upcase_atom = function(t, s)
{
    Utils.must_be_atom(t);
    return this.unify(s, new AtomTerm(t.value.toUpperCase()));
}

module.exports.downcase_atom = function(t, s)
{
    Utils.must_be_atom(t);
    return this.unify(s, new AtomTerm(t.value.toLowerCase()));
}

function toFloat(arg)
{
    var v = Arithmetic.evaluate(arg);
    console.log("Arg: " + v);
    if (v instanceof IntegerTerm || v instanceof FloatTerm)
        return Number(v.value);
    else if (v instanceof BigIntegerTerm)
        return Number(v.value.valueOf());
    else if (v instanceof RationalTerm)
        return v.value.toFloat();
    else
        Errors.typeError(Constants.floatAtom, v);
}


function format(env, sink, formatString, formatArgs)
{
    Utils.must_be_atom(formatString);
    var a = 0;
    var input = formatString.value;
    var output = '';
    var radix = -1;
    var nextArg = function()
    {
        if (formatArgs instanceof CompoundTerm && formatArgs.functor.equals(Constants.listFunctor))
        {
            var nextArg = formatArgs.args[0].dereference();
            formatArgs = formatArgs.args[1].dereference();
            return nextArg;
        }
        Errors.formatError(new AtomTerm("not enough arguments"));
    }
    for (i = 0; i < input.length; i++)
    {
        if (input.charAt(i) == '~')
        {
            if (input.charAt(i+1) == '~')
                output += "~";
            else
            {
                i++;
                radix = -1;
                while(true)
                {
                    switch(input.charAt(i))
                    {
                        case 'a': // atom
                        {
                            var atom = nextArg();
                            Utils.must_be_atom(atom);
                            output += atom.value;
                            break;
                        }
                        case 'c': // character code
                        {
                            var code = nextArg();
                            Utils.must_be_character_code(code);
                            output += String.fromCharCode(code.value);
                            break;
                        }
                        case 'd': // decimal
                        {
                            var d = nextArg();
                            if (d instanceof IntegerTerm)
                                output += d.value;
                            else if (d instanceof BigIntegerTerm)
                                output += d.value.toString();
                            else
                                Errors.typeError(Constants.integerAtom, d);
                            break;
                        }
                        case 'D': // decimal with separators
                        {
                            var d = nextArg();
                            var tmp = ""
                            if (d instanceof IntegerTerm)
                                tmp = String(d.value);
                            else if (d instanceof BigIntegerTerm)
                                tmp = d.value.toString();
                            else
                                Errors.typeError(Constants.integerAtom, d);
                            var i = tmp.length % 3;
                            for (var j = 0; j < tmp.length; j+=i)
                            {
                                output += tmp.substring(j, j+i);
                                if (i+3 < tmp.length)
                                    output += ",";
                                i = 3;
                            }
                            break;
                        }
                        case 'e': // floating point as exponential
                        {
                            var d = nextArg();
                            var f = toFloat(d);
                            output += f.toExponential(radix == -1?6:radix);
                            break;
                        }
                        case 'E': // floating point as exponential in upper-case
                        {
                            var d = nextArg();
                            var f = toFloat(d);
                            output += f.toExponential(radix == -1?6:radix).toUpperCase();
                            break;                            
                        }
                        case 'f': // floating point as non-exponential
                        {
                            var d = nextArg();
                            var f = toFloat(d);
                            console.log("Radix: " + radix);
                            output += f.toFixed(radix == -1?6:radix);
                            break;
                        }
                        case 'g': // shorter of e or f
                        {
                            var d = nextArg();
                            var f = toFloat(d);
                            e = f.toExponential(radix == -1?6:radix);
                            f = f.toFixed(radix == -1?6:radix);
                            if (e.length < f.length)
                                output += e;
                            else
                                output += f;
                            break;
                        }
                        case 'G': // shorter of E or f
                        {
                            var d = nextArg();
                            var f = toFloat(d);
                            e = f.toExponential(radix == -1?6:radix).toUpperCase();
                            f = f.toFixed(radix == -1?6:radix);
                            if (e.length < f.length)
                                output += e;
                            else
                                output += f;
                            break;
                        }
                        case 'i': // ignore
                        {
                            nextArg();
                            continue;
                        }
                        case 'I': // integer with _ separator
                        {
                            var d = nextArg();
                            var tmp = ""
                            if (d instanceof IntegerTerm)
                                tmp = String(d.value);
                            else if (d instanceof BigIntegerTerm)
                                tmp = d.value.toString();
                            else
                                Errors.typeError(Constants.integerAtom, d);
                            var i = tmp.length % 3;
                            for (var j = 0; j < tmp.length; j+=i)
                            {
                                output += tmp.substring(j, j+i);
                                if (i+3 < tmp.length)
                                    output += "_";
                                i = 3;
                            }
                            break;
                        }
                        case 'n': // Newline
                        {
                            output += "\n";
                            break;
                        }
                        case 'N': // Soft newline
                        {
                            if (!output.charAt(output.length-1) == "\n")
                                output += "\n";
                            break;
                        }
                        case 'p': // print
                        {
                            // For now just treat this as write/1
                            output += TermWriter.formatTerm({numbervars: true}, 1200, nextArg());
                            break;
                        }
                        case 'q': // writeq
                        {
                            output += TermWriter.formatTerm({numbervars: true, quoted:true}, 1200, nextArg());
                            break;

                        }
                        case 'r': // radix
                        {
                            var d = nextArg();
                            var tmp = ""
                            if (d instanceof IntegerTerm)
                                tmp = d.value.toString(radix);
                            else if (d instanceof BigIntegerTerm)
                                tmp = d.value.toString(radix);
                            else
                                Errors.typeError(Constants.integerAtom, d);
                            output += tmp.toLowerCase();
                            break;
                        }
                        case 'R': // radix in uppercase
                        {
                            var d = nextArg();
                            var tmp = ""
                            if (d instanceof IntegerTerm)
                                tmp = d.value.toString(radix);
                            else if (d instanceof BigIntegerTerm)
                                tmp = d.value.toString(radix);
                            else
                                Errors.typeError(Constants.integerAtom, d);
                            output += tmp.toUpperCase();
                            break;
                        }
                        case 's': // string
                        {
                            var d = nextArg();
                            output += "<not implemented>";
                            break;
                        }
                        case '@': // execute
                        {
                            var d = nextArg();
                            output += "<not implemented>";
                            break;
                        }
                        case 't': // tab
                        {
                            var d = nextArg();
                            output += "<not implemented>";
                            break;
                        }
                        case '|': // tab-stop
                        {
                            var d = nextArg();
                            output += "<not implemented>";
                            break;
                        }
                        case '+': // tab-stop
                        {
                            var d = nextArg();
                            output += "<not implemented>";
                            break;
                        }
                        case 'w': // write
                        {
                            output += TermWriter.formatTerm({numbervars: true}, 1200, nextArg());
                            break;
                        }
                        // These things relate to parsing arguments
                        case '*':
                        {
                            var v = nextArg();
                            Utils.must_be_integer(v);
                            radix = v.value;
                            continue;
                        }
                        case '0':
                        case '1':
                        case '2':
                        case '3':
                        case '4':
                        case '5':
                        case '6':
                        case '7':
                        case '8':
                        case '9':
                        {
                            radix = '';
                            while (input.charAt(i) >= '0' && input.charAt(i) <= '9')
                                radix = radix + input.charAt(i++);
                            radix = Number(radix);
                            continue;
                        }
                        default:
                        {
                            Errors.formatError(new AtomTerm("No such format character: " + input.charAt(i)));
                        }

                    }
                    break;
                }

            }
        }
        else
            output += input.charAt(i);
    }
    if (sink instanceof CompoundTerm && sink.functor.equals(Constants.atomFunctor))
    {
        return env.unify(sink.args[0], new AtomTerm(output));
    }
    var bufferObject = Stream.stringBuffer(output.toString());
    return sink.value.write(sink.value, 0, bufferObject.buffer.length, bufferObject.buffer) >= 0;
}

module.exports.format = [
    function(sink, formatString, formatArgs)
    {
        return format(this, sink, formatString, formatArgs);
    },
    function(formatString, formatArgs)
    {
        return format(this, this.streams.current_output.term, formatString, formatArgs);
    }];


module.exports.sleep = function(interval)
{
    var i = 0;
    if (interval instanceof IntegerTerm)
        i = interval.value * 1000;
    else if (interval instanceof FloatTerm)
        i = interval.value * 1000;
    else
        Errors.typeError(Constants.numberAtom, interval);
    if (i < 0)
        Errors.domainErrorFunctor(Constants.notLessThanZeroAtom, interval);
    var resume = this.yield_control();
    setTimeout(function() {resume(true);}, i);
    return "yield";
}

module.exports.qqq = function()
{
    return true;
}
