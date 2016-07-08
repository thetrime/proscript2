// This module contains non-ISO extensions
var Constants = require('./constants');
var IntegerTerm = require('./integer_term');
var Term = require('./term');

module.exports.term_variables = function(t, vt)
{
    // FIXME: term_variables is not defined!
    return this.unify(Term.term_from_list(term_variables(t), Constants.emptyListAtom), vt);
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

