var Constants = require('./constants.js');
var CompoundTerm = require('./compound_term.js');
var AtomTerm = require('./atom_term.js');
var VariableTerm = require('./variable_term.js');
var IntegerTerm = require('./integer_term.js');
var FloatTerm = require('./float_term.js');

module.exports.must_be_bound = function(t)
{
    if (t instanceof VariableTerm)
        Errors.instantiationError(t);
}

module.exports.must_be_atom = function(t)
{
    if (!(t instanceof AtomTerm))
        Errors.typeError(Constants.atomAtom, t);
}

module.exports.must_be_integer = function(t)
{
    if (!(t instanceof IntegerTerm))
        Errors.typeError(Constants.atomAtom, t);
}
