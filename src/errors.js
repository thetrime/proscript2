var CompoundTerm = require('./compound_term');
var AtomTerm = require('./atom_term');
var Constants = require('./constants');
var VariableTerm = require('./variable_term');
var Functor = require('./functor');
var IntegerTerm = require('./integer_term');

var errorFunctor = new Functor(new AtomTerm("error"), 2);
var typeErrorFunctor = new Functor(new AtomTerm("type_error"), 2);
var domainErrorFunctor = new Functor(new AtomTerm("domain_error"), 2);
var representationErrorFunctor = new Functor(new AtomTerm("representation_error"), 1);
var existenceErrorFunctor = new Functor(new AtomTerm("existence_error"), 2);
var permissionErrorFunctor = new Functor(new AtomTerm("permission_error"), 3);
var evaluationErrorFunctor = new Functor(new AtomTerm("evaluation_error"), 1);
var syntaxErrorFunctor = new Functor(new AtomTerm("syntax_error"), 1);
var formatErrorFunctor = new Functor(new AtomTerm("format_error"), 1);
var ioErrorFunctor = new Functor(new AtomTerm("io_error"), 2);
var systemErrorFunctor = new AtomTerm("system_error");
var instantiationErrorAtom = new AtomTerm("instantiation_error");
var integerOverflowAtom = new AtomTerm("integer_overflow");
var floatOverflowAtom = new AtomTerm("float_overflow");
var zeroDivisorAtom = new AtomTerm("zero_divisor");

module.exports.instantiationError = function()
{
    throw new CompoundTerm(errorFunctor, [instantiationErrorAtom, new VariableTerm()]);
}

module.exports.systemError = function(t)
{
    throw new CompoundTerm(errorFunctor, [systemErrorFunctor, t]);
}

module.exports.typeError = function(expected, actual)
{
    //console.log(new Error().stack);
    throw new CompoundTerm(errorFunctor, [new CompoundTerm(typeErrorFunctor, [expected, actual]), new VariableTerm()]);
}

module.exports.domainError = function(domain, value)
{
    throw new CompoundTerm(errorFunctor, [new CompoundTerm(domainErrorFunctor, [domain, value]), new VariableTerm()]);
}

module.exports.permissionError = function(operation, type, culprit)
{
    throw new CompoundTerm(errorFunctor, [new CompoundTerm(permissionErrorFunctor, [operation, type, culprit]), new VariableTerm()]);
}

module.exports.representationError = function(flag) // The ISO standard does NOT include what has violated the representation, sadly. We pass it in anyway
{
    throw new CompoundTerm(errorFunctor, [new CompoundTerm(representationErrorFunctor, [flag]), new VariableTerm()]);
}

// non-ISO
module.exports.existenceError = function(type, value)
{
    throw new CompoundTerm(errorFunctor, [new CompoundTerm(existenceErrorFunctor, [type, value]), new VariableTerm()]);
}


module.exports.integerOverflow = function()
{
    throw new CompoundTerm(errorFunctor, [new CompoundTerm(evaluationErrorFunctor, [integerOverflowAtom]), new VariableTerm()]);
}

module.exports.floatOverflow = function()
{
    throw new CompoundTerm(errorFunctor, [new CompoundTerm(evaluationErrorFunctor, [floatOverflowAtom]), new VariableTerm()]);
}

module.exports.zeroDivisor = function()
{
    throw new CompoundTerm(errorFunctor, [new CompoundTerm(evaluationErrorFunctor, [zeroDivisorAtom]), new VariableTerm()]);
}

module.exports.syntaxError = function(message)
{
    throw new CompoundTerm(errorFunctor, [new CompoundTerm(syntaxErrorFunctor, [message]), new VariableTerm()]);
}

module.exports.ioError = function(what, where) // Non-ISO
{
    throw new CompoundTerm(errorFunctor, [new CompoundTerm(ioErrorFunctor, [what, where]), new VariableTerm()]);
}

module.exports.formatError = function(message) // Non-ISO
{
    throw new CompoundTerm(errorFunctor, [new CompoundTerm(formatErrorFunctor, [message]), new VariableTerm()]);
}
