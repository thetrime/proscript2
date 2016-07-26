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
var CTable = require('./ctable');

module.exports.term_variables = function(t, vt)
{
    // FIXME: term_variables is not defined!
    return this.unify(Utils.term_from_list(term_variables(t), Constants.emptyListAtom), vt);
}

module.exports["$det"] = function()
{
    // Since $det is an actual predicate that gets called, we want to look at the /parent/ frame to see if THAT was deterministic
    // Obviously $det/0 itself is always deterministic!
    console.log("Parent frame " + this.currentFrame.parent.functor + " has choicepoint of " + this.currentFrame.parent.choicepoint + ", there are " + this.CP + " active choicepoints");
    return this.currentFrame.parent.choicepoint == this.CP;
}

module.exports["$choicepoint_depth"] = function(t)
{
    return this.unify(t, IntegerTerm.get(this.CP));
}

module.exports.keysort = function(unsorted, sorted)
{
    var list = Utils.to_list(unsorted);

    for (var i = 0; i < list.length; i++)
    {
        if (!(TAGOF(list[i]) == CompoundTag && FUNCTOROF(list[i]) == Constants.pairFunctor))
            Errors.typeError(Constants.pairAtom, list[i]);
        list[i] = {position: i, value: list[i]};
    }
    list.sort(function(a,b)
              {
                  var d = Utils.difference(ARGOF(a.value, 0), ARGOF(b.value,0));
                  // Ensure that the sort is stable
                  if (d == 0)
                      return a.position - b.position;
                  return d;
              });
    var result = Constants.emptyListAtom;
    for (var i = list.length-1; i >= 0; i--)
        result = CompoundTerm.create(Constants.listFunctor, [list[i].value, result]);
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
        if (last == null || list[i] != last)
        {
            last = DEREF(list[i]);
            result = CompoundTerm.create(Constants.listFunctor, [last, result]);
        }
    }
    return this.unify(sorted, result);
}

module.exports.is_list = function(t)
{
    while (TAGOF(t) == CompoundTag)
    {
        if (FUNCTOROF(t) == Constants.listFunctor)
            t = ARGOF(t, 1);
        else
            return false;
    }
    return Constants.emptyListAtom == t;
}

module.exports.upcase_atom = function(t, s)
{
    Utils.must_be_atom(t);
    return this.unify(s, AtomTerm.get((CTable.get(t).value).toUpperCase()));
}

module.exports.downcase_atom = function(t, s)
{
    Utils.must_be_atom(t);
    return this.unify(s, AtomTerm.get((CTable.get(t).value).toLowerCase()));
}

function toFloat(arg)
{
    var v = Arithmetic.evaluate(arg);
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
    var input = CTable.get(formatString).value;
    var output = '';
    var radix = -1;
    var nextArg = function()
    {
        if (TAGOF(formatArgs) == CompoundTag && FUNCTOROF(formatArgs) == Constants.listFunctor)
        {
            var nextArg = ARGOF(formatArgs, 0);
            formatArgs = ARGOF(formatArgs, 1);
            return nextArg;
        }
        Errors.formatError(AtomTerm.get("not enough arguments"));
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
                            output += CTable.get(atom).value;
                            break;
                        }
                        case 'c': // character code
                        {
                            var code = nextArg();
                            Utils.must_be_character_code(code);
                            output += String.fromCharCode(CTable.get(atom).value);
                            break;
                        }
                        case 'd': // decimal
                        {
                            var d = nextArg();
                            if (TAGOF(d) == ConstantTag)
                            {
                                var dx = CTable.get(d);
                                if (dx instanceof IntegerTerm)
                                    output += dx.value;
                                else if (dx instanceof BigIntegerTerm)
                                    output += dx.value.toString();
                                else
                                    Errors.typeError(Constants.integerAtom, d);
                            }
                            else
                                Errors.typeError(Constants.integerAtom, d);
                            break;
                        }
                        case 'D': // decimal with separators
                        {
                            var d = nextArg();
                            if (TAGOF(d) == ConstantTag)
                            {
                                var dx = CTable.get(d);
                                var tmp = ""
                                if (dx instanceof IntegerTerm)
                                    tmp = String(dx.value);
                                else if (d instanceof BigIntegerTerm)
                                    tmp = dx.value.toString();
                                else
                                    Errors.typeError(Constants.integerAtom, d);
                            }
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
                            if (TAGOF(d) == CompoundTag)
                            {
                                var dx = CTable.get(d);
                                if (dx instanceof IntegerTerm)
                                    tmp = String(dx.value);
                                else if (dx instanceof BigIntegerTerm)
                                    tmp = dx.value.toString();
                                else
                                    Errors.typeError(Constants.integerAtom, d);
                            }
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
                            if (TAGOF(d) == ConstantTag)
                            {
                                var dx = CTable.get(d);
                                if (dx instanceof IntegerTerm)
                                    tmp = dx.value.toString(radix);
                                else if (dx instanceof BigIntegerTerm)
                                    tmp = dx.value.toString(radix);
                                else
                                    Errors.typeError(Constants.integerAtom, d);
                            }
                            else
                                Errors.typeError(Constants.integerAtom, d);
                            output += tmp.toLowerCase();
                            break;
                        }
                        case 'R': // radix in uppercase
                        {
                           var d = nextArg();
                            var tmp = ""
                            if (TAGOF(d) == ConstantTag)
                            {
                                var dx = CTable.get(d);
                                if (dx instanceof IntegerTerm)
                                    tmp = dx.value.toString(radix);
                                else if (dx instanceof BigIntegerTerm)
                                    tmp = dx.value.toString(radix);
                                else
                                    Errors.typeError(Constants.integerAtom, d);
                            }
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
                            radix = CTable.get(v).value;
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
                            Errors.formatError(AtomTerm.get("No such format character: " + input.charAt(i)));
                        }

                    }
                    break;
                }

            }
        }
        else
            output += input.charAt(i);
    }
    if (TAGOF(sink) == CompoundTag && FUNCTOROF(sink) == Constants.atomFunctor)
    {
        return env.unify(ARGOF(sink, 0), AtomTerm.get(output));
    }
    var bufferObject = Stream.stringBuffer(output.toString());
    var stream = CTable.get(sink).value;
    return stream.write(stream, 0, bufferObject.buffer.length, bufferObject.buffer) >= 0;
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
    if (TAGOF(interval) == ConstantTag)
    {
        var ix = CTable.get(interval);
        if (ix instanceof IntegerTerm)
            i = ix.value * 1000;
        else if (ix instanceof FloatTerm)
            i = ix.value * 1000;
        else
            Errors.typeError(Constants.numberAtom, interval);
    }
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
