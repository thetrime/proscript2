"use strict";
exports=module.exports;

var CompoundTerm = require('./compound_term');
var AtomTerm = require('./atom_term');
var VariableTerm = require('./variable_term');
var Functor = require('./functor');

var errorFunctor = Functor.get(AtomTerm.get("error"), 2);
var typeErrorFunctor = Functor.get(AtomTerm.get("type_error"), 2);
var domainErrorFunctor = Functor.get(AtomTerm.get("domain_error"), 2);
var representationErrorFunctor = Functor.get(AtomTerm.get("representation_error"), 1);
var existenceErrorFunctor = Functor.get(AtomTerm.get("existence_error"), 2);
var permissionErrorFunctor = Functor.get(AtomTerm.get("permission_error"), 3);
var evaluationErrorFunctor = Functor.get(AtomTerm.get("evaluation_error"), 1);
var syntaxErrorFunctor = Functor.get(AtomTerm.get("syntax_error"), 1);
var formatErrorFunctor = Functor.get(AtomTerm.get("format_error"), 1);
var ioErrorFunctor = Functor.get(AtomTerm.get("io_error"), 2);
var systemErrorFunctor = AtomTerm.get("system_error");
var instantiationErrorAtom = AtomTerm.get("instantiation_error");
var integerOverflowAtom = AtomTerm.get("integer_overflow");
var floatOverflowAtom = AtomTerm.get("float_overflow");
var zeroDivisorAtom = AtomTerm.get("zero_divisor");

module.exports.instantiationError = function()
{
    //console.log(new Error().stack);
    throw CompoundTerm.create(errorFunctor, [instantiationErrorAtom, new VariableTerm()]);
}

module.exports.systemError = function(t)
{
    throw CompoundTerm.create(errorFunctor, [CompoundTerm.create(systemErrorFunctor, [t]), new VariableTerm()]);
}

module.exports.makeSystemError = function(t)
{
    return CompoundTerm.create(errorFunctor, [CompoundTerm.create(systemErrorFunctor, [t]), new VariableTerm()]);
}

module.exports.typeError = function(expected, actual)
{
   //console.log(new Error().stack);
   throw CompoundTerm.create(errorFunctor, [CompoundTerm.create(typeErrorFunctor, [expected, actual]), new VariableTerm()]);
}

module.exports.domainError = function(domain, value)
{
    throw CompoundTerm.create(errorFunctor, [CompoundTerm.create(domainErrorFunctor, [domain, value]), new VariableTerm()]);
}

module.exports.permissionError = function(operation, type, culprit)
{
    throw CompoundTerm.create(errorFunctor, [CompoundTerm.create(permissionErrorFunctor, [operation, type, culprit]), new VariableTerm()]);
}

module.exports.representationError = function(flag) // The ISO standard does NOT include what has violated the representation, sadly. We pass it in anyway
{
    throw CompoundTerm.create(errorFunctor, [CompoundTerm.create(representationErrorFunctor, [flag]), new VariableTerm()]);
}

// non-ISO
module.exports.existenceError = function(type, value)
{
    throw CompoundTerm.create(errorFunctor, [CompoundTerm.create(existenceErrorFunctor, [type, value]), new VariableTerm()]);
}


module.exports.integerOverflow = function()
{
    throw CompoundTerm.create(errorFunctor, [CompoundTerm.create(evaluationErrorFunctor, [integerOverflowAtom]), new VariableTerm()]);
}

module.exports.floatOverflow = function()
{
    throw CompoundTerm.create(errorFunctor, [CompoundTerm.create(evaluationErrorFunctor, [floatOverflowAtom]), new VariableTerm()]);
}

module.exports.zeroDivisor = function()
{
    throw CompoundTerm.create(errorFunctor, [CompoundTerm.create(evaluationErrorFunctor, [zeroDivisorAtom]), new VariableTerm()]);
}

module.exports.syntaxError = function(message)
{
    throw CompoundTerm.create(errorFunctor, [CompoundTerm.create(syntaxErrorFunctor, [message]), new VariableTerm()]);
}

module.exports.ioError = function(what, where) // Non-ISO
{
    throw CompoundTerm.create(errorFunctor, [CompoundTerm.create(ioErrorFunctor, [what, where]), new VariableTerm()]);
}

module.exports.formatError = function(message) // Non-ISO
{
    throw CompoundTerm.create(errorFunctor, [CompoundTerm.create(formatErrorFunctor, [message]), new VariableTerm()]);
}
