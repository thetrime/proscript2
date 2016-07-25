"use strict";
exports=module.exports;

var Errors = require('./errors.js');
var Constants = require('./constants.js');
var AtomTerm = require('./atom_term.js');
var CompoundTerm = require('./compound_term.js');
var IntegerTerm = require('./integer_term.js');
var CTable = require('./ctable.js');

var doubleQuotesAtom = AtomTerm.get("double_quotes");
var unknownAtom = AtomTerm.get("unknown");
var charConversionAtom = AtomTerm.get("char_conversion");
var debugAtom = AtomTerm.get("debug");


function flag_bounded(set, value)
{
    if (set) Errors.permissionError(Constants.modifyAtom, Constants.flagAtom, Constants.boundedAtom);
    return Constants.trueAtom;
}

function flag_max_integer(set, value)
{
    if (set) Errors.permissionError(Constants.modifyAtom, Constants.flagAtom, Constants.maxIntegerAtom);
    return IntegerTerm.get(268435455);
}

function flag_min_integer(set, value)
{
    if (set) Errors.permissionError(Constants.modifyAtom, Constants.flagAtom, Constants.minIntegerAtom);
    return IntegerTerm.get(536870911);
}

function flag_integer_rounding_function(set, value)
{
    if (set) Errors.permissionError(Constants.modifyAtom, Constants.flagAtom, Constants.integerRoundingFunctionAtom);
    return Constants.towardZeroAtom;
}

function flag_char_conversion(set, value)
{
    if (set) 
    {
        if (TAGOF(value) == ConstantTag && CTable.get(value) instanceof AtomTerm && CTable.get(value).value == "on")
            prolog_flag_values.char_conversion = true;
        else if (TAGOF(value) == ConstantTag && CTable.get(value) instanceof AtomTerm && CTable.get(value).value == "off")
            prolog_flag_values.char_conversion = false;
        else
            Errors.domainError(Constants.flagValueAtom, CompoundTerm.create(Constants.addFunctor, [charConversionAtom, value]));
        return true;
    }
    return prolog_flag_values.char_conversion?Constants.onAtom:Constants.offAtom;
}

function flag_debug(set, value)
{
    if (set) 
    {
        if (TAGOF(value) == ConstantTag && CTable.get(value) instanceof AtomTerm && CTable.get(value).value == "on")
            prolog_flag_values.debug = true;
        else if (TAGOF(value) == ConstantTag && CTable.get(value) instanceof AtomTerm && CTable.get(value).value == "off")
            prolog_flag_values.debug = false;
        else
            Errors.domainError(Constants.flagValueAtom, CompoundTerm.create(Constants.addFunctor, [debugAtom, value]));
        return true;
    }
    return prolog_flag_values.debug?Constants.onAtom:Constants.offAtom;
}

function flag_max_arity(set, value)
{
    if (set) Errors.permissionError(Constants.modifyAtom, Constants.flagAtom, Constants.maxArityAtom);
    return Constants.unboundedAtom;
}

function flag_unknown(set, value)
{
    if (set) 
    {
        if (TAGOF(value) == ConstantTag && CTable.get(value) instanceof AtomTerm && CTable.get(value).value == "error")
            prolog_flag_values.unknown = "error";
        else if (TAGOF(value) == ConstantTag && CTable.get(value) instanceof AtomTerm && CTable.get(value).value == "fail")
            prolog_flag_values.unknown = "fail";
        else if (TAGOF(value) == ConstantTag && CTable.get(value) instanceof AtomTerm && CTable.get(value).value == "warning")
            prolog_flag_values.unknown = "warning";
        else
            Errors.domainError(Constants.flagValueAtom, CompoundTerm.create(Constants.addFunctor, [unknownAtom, value]));
        return true;
    }
    return AtomTerm.get(prolog_flag_values.unknown);
}

function flag_double_quotes(set, value)
{
    if (set) 
    {
        if (TAGOF(value) == ConstantTag && CTable.get(value) instanceof AtomTerm && CTable.get(value).value == "chars")
            prolog_flag_values.double_quotes = "chars";
        else if (TAGOF(value) == ConstantTag && CTable.get(value) instanceof AtomTerm && CTable.get(value).value == "codes")
            prolog_flag_values.double_quotes = "codes";
        else if (TAGOF(value) == ConstantTag && CTable.get(value) instanceof AtomTerm && CTable.get(value).value == "atom")
            prolog_flag_values.double_quotes = "atom";
        else
            Errors.domainError(Constants.flagValueAtom, CompoundTerm.create(Constants.addFunctor, [doubleQuotesAtom, value]));
        return true;
    }
    return AtomTerm.get(prolog_flag_values.double_quotes);
}


var prolog_flags = [{name:"bounded", fn:flag_bounded},
                    {name:"max_integer", fn:flag_max_integer},
                    {name:"min_integer", fn:flag_min_integer},
                    {name:"integer_rounding_function", fn:flag_integer_rounding_function},
                    {name:"char_conversion", fn:flag_char_conversion},
                    {name:"debug", fn:flag_debug},
                    {name:"max_arity", fn:flag_max_arity},
                    {name:"unknown", fn:flag_unknown},
                    {name:"double_quotes", fn:flag_double_quotes}];

var prolog_flag_values = {char_conversion: false,
                          debug: false,
                          unknown: "error",
			  double_quotes: "codes"};

module.exports = {flags: prolog_flags,
		  values: prolog_flag_values};
