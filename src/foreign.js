// This module contains non-ISO extensions
var Constants = require('./constants');
var IntegerTerm = require('./integer_term');
var AtomTerm = require('./atom_term');
var CompoundTerm = require('./compound_term');
var Utils = require('./utils');
var util = require('util');
var Errors = require('./errors');

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

function format(sink, formatString, formatArgs)
{
    Utils.must_be_atom(formatString);
    var a = 0;
    var input = formatString.value;
    var output = '';
    var radix = 0;
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
                        }
                        case 'd': // decimal
                        {
                        }
                        case 'D': // decimal with separators
                        {
                        }
                        case 'e': // floating point as exponential
                        {
                        }
                        case 'E': // floating point as exponential in upper-case
                        {
                        }
                        case 'f': // floating point as non-exponential
                        {
                        }
                        case 'g': // shorter of e or f
                        {
                        }
                        case 'G': // shorter of E or f
                        {
                        }
                        case 'i': // ignore
                        {
                            nextArg();
                            continue;
                        }
                        case 'I': // integer with _ separator
                        {
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
                        }
                        case 'q': // writeq
                        {
                        }
                        case 'r': // radix
                        {
                        }
                        case 'R': // radix in uppercase
                        {
                        }
                        case 's': // string
                        {
                        }
                        case '@': // execute
                        {
                        }
                        case 't': // tab
                        {
                        }
                        case '|': // tab-stop
                        {
                        }
                        case '+': // tab-stop
                        {
                        }
                        case 'w': // write
                        {
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
                            radix = input.charAt(i) - '0';
                            while (input.charAt(i) >= '0' && input.charAt(i) <= '9')
                                radix = 10 * radix + input.charAt(i++) - '0';
                            continue;
                        }
                        default:
                        {
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
        return this.unify(sink.args[0], new AtomTerm(output));
    var bufferObject = Stream.stringBuffer(output.toString());
    return sink.value.write(stream, 0, bufferObject.buffer.length, bufferObject.buffer) >= 0;
}

module.exports.format = [
    function(sink, formatString, formatArgs)
    {
        return format(sink, formatString, formatArgs);
    },
    function(formatString, formatArgs)
    {
        return format(this.streams.current_output, formatString, formatArgs);
    }];


module.exports.qqq = function()
{
    console.log("xChoicepoints: " + util.inspect(this.choicepoints));
    return true;
}

