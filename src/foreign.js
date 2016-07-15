// This module contains non-ISO extensions
var Constants = require('./constants');
var IntegerTerm = require('./integer_term');
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
    }
    return Constants.emptyListAtom.equals(t);
}

module.exports.qqq = function()
{
    console.log("xChoicepoints: " + util.inspect(this.choicepoints));
    return true;
}

