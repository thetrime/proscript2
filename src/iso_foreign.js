"use strict";
exports=module.exports;


var Constants = require('./constants.js');
var CompoundTerm = require('./compound_term.js');
var AtomTerm = require('./atom_term.js');
var VariableTerm = require('./variable_term.js');
var IntegerTerm = require('./integer_term.js');
var BigIntegerTerm = require('./biginteger_term.js');
var RationalTerm = require('./rational_term.js');
var BlobTerm = require('./blob_term.js');
var FloatTerm = require('./float_term.js');
var Parser = require('./parser.js');
var PrologFlag = require('./prolog_flag.js');
var Utils = require('./utils.js');
var Errors = require('./errors.js');
var CTable = require('./ctable.js');


function acyclic_term(t)
{
    var visited = [];
    var stack = [DEREF(t)];
    while (stack.length != 0)
    {
        var arg = stack.pop();
        if (TAGOF(arg) == VariableTag)
        {
            var needle = DEREF(arg);
            for (var i = 0; i < visited.length; i++)
            {
                if (visited[i] === needle)
                {
                    return false;
                }
            }
        }
        else if (TAGOF(arg) == CompoundTag)
        {
            visited.push(arg);
            var functor = CTable.get(FUNCTOROF(arg));
            for (var i = 0; i < functor.arity; i++)
                stack.push(ARGOF(arg, i));
        }
    }
    return true;
}



// ISO built-in predicates
// 8.2.1 (=)/2 (this is always compiled though)
module.exports["="] = function(a, b)
{
    return this.unify(a,b);
}
// 8.2.2 unify_with_occurs_check/2
module.exports.unify_with_occurs_check = function(a, b)
{
    return this.unify(a, b) && acyclic_term(a);
}
// 8.2.4 (\=)/2 is compiled directly into VM opcodes, but this is roughly what the could SHOULD do
module.exports["\\="] = function(a, b)
{
    var b = this.create_choicepoint();
    var result = !this.unify(a, b);
    this.backtrack(b);
    return result;
}
// 8.3.1 var/1
module.exports["var"] = function(a)
{
    return (TAGOF(a) == VariableTag);
}
// 8.3.2 atom/1
module.exports["atom"] = function(a)
{
    return (TAGOF(a) == ConstantTag && CTable.get(a) instanceof AtomTerm);
}
// 8.3.3 integer/1
module.exports["integer"] = function(a)
{
    return (TAGOF(a) == ConstantTag && CTable.get(a) instanceof IntegerTerm);
}
// 8.3.4 float/1
module.exports["float"] = function(a)
{
    return (TAGOF(a) == ConstantTag && CTable.get(a) instanceof FloatTerm);
}
// 8.3.5 atomic/1
module.exports.atomic = function(a)
{
    return (TAGOF(a) == ConstantTag);
}
// 8.3.6 compound/1
module.exports.compound = function(a)
{
    return (TAGOF(a) == CompoundTag);
}
// 8.3.7 nonvar/1
module.exports.nonvar = function(a)
{
    return (TAGOF(a) != VariableTag);
}
// 8.3.8 number/1
module.exports.number = function(a)
{
    if (TAGOF(a) != ConstantTag)
        return false;
    a = CTable.get(a);
    return ((a instanceof IntegerTerm) ||
            (a instanceof FloatTerm) ||
            (a instanceof BigIntegerTerm) ||
            (a instanceof RationalTerm));
}
// 8.4.1 term-comparison
module.exports["@=<"] = function(a, b)
{
    return Utils.difference(a, b) <= 0;
}
module.exports["=="] = function(a, b)
{
    return Utils.difference(a, b) == 0;
}
module.exports["\\=="] = function(a, b)
{
    return Utils.difference(a, b) != 0;
}
module.exports["@<"] = function(a, b)
{
    return Utils.difference(a, b) < 0;
}
module.exports["@>"] = function(a, b)
{
    return Utils.difference(a, b) > 0;
}
module.exports["@>="] = function(a, b)
{
    return Utils.difference(a, b) >= 0;
}
// 8.5.1 functor/3
module.exports.functor = function(term, name, arity)
{
    if (TAGOF(term) == VariableTag)
    {
        // Construct a term
        Utils.must_be_positive_integer(arity);
        Utils.must_be_bound(name);
        arity = CTable.get(arity);
        if (arity.value == 0)
        {
            return this.unify(term, name);
        }
        var args = new Array(arity.value);
        for (var i = 0; i < args.length; i++)
            args[i] = MAKEVAR();
        if (TAGOF(name) == CompoundTag)
            Errors.typeError(Constants.atomicAtom, name);
        Utils.must_be_atom(name);
        return this.unify(term, CompoundTerm.create(name, args));
    }
    else if (TAGOF(term) == ConstantTag)
    {
        return this.unify(name, term) && this.unify(arity, IntegerTerm.get(0));
    }
    else if (TAGOF(term) == CompoundTag)
    {
        var functor = CTable.get(FUNCTOROF(term));
        return this.unify(name, functor.name) && this.unify(arity, IntegerTerm.get(functor.arity));
    }
    else
        Errors.typeError(Constants.compoundAtom, term);
}
// 8.5.2 arg/3
module.exports.arg = function(n, term, arg)
{
    // ISO only requires the +,+,? mode
    // We also support -,+,? since the bagof implementation requires it
    Utils.must_be_bound(term);
    if (TAGOF(term) != CompoundTag)
        Errors.typeError(Constants.compoundAtom, term);
    if (TAGOF(n) == VariableTag)
    {
        // -,+,? mode
        var functor = CTable.get(FUNCTOROF(term));
        var index = this.foreign || 0;
        if (index + 1 < functor.arity)
            this.create_choicepoint(index+1);
        if (index >= functor.arity)
            return false;
        return (this.unify(n, IntegerTerm.get(index+1)) && // arg is 1-based, but all our terms are 0-based
                this.unify(ARGOF(term, index), arg));
    }
    else
    {
        Utils.must_be_positive_integer(n);
        n = CTable.get(n);
        if (n.value > CTable.get(FUNCTOROF(term)).arity)
            return false; // N is too big
        if (n < 1)
            return false;
        return this.unify(ARGOF(term, n.value-1), arg);
    }
}
// 8.5.3 (=..)/2
module.exports["=.."] = function(term, univ)
{
    // CHECKME: More errors should be checked
    if (TAGOF(term) == ConstantTag)
    {
        return this.unify(univ, Utils.from_list([term]));
    }
    if (TAGOF(term) == CompoundTag)
    {
        var functor = CTable.get(FUNCTOROF(term));
        var list = [functor.name];
        for (var i = 0; i < functor.arity; i++)
            list.push(ARGOF(term, i));
        return this.unify(univ, Utils.from_list(list));
    }
    if (TAGOF(term) == VariableTag)
    {
        var list = Utils.to_list(univ);
        if (list.length == 0)
            Errors.domainError(Constants.nonEmptyListAtom, univ);
        var fname = list.shift();
        Utils.must_be_bound(fname);
        return this.unify(term, CompoundTerm.create(fname, list));
    }
    else
        Errors.typeError(Constants.compoundAtom, term);
}
// 8.5.4 copy_term/2
module.exports.copy_term = function(term, copy)
{
    return this.unify(this.copyTerm(term), copy);
}
// 8.8.1 clause/2
module.exports.clause = function(head, body)
{
    // FIXME: Assumes current module
    // FIXME: Should allow Module:Head

    // This is silly, but ok
    if (TAGOF(body) == ConstantTag)
    {
        var t = CTable.get(body);
        if ((t instanceof IntegerTerm) || (body instanceof FloatTerm))
            Errors.typeError(Constants.callableAtom, body);
    }

    var functor = Utils.head_functor(head);
    var predicate = this.currentModule.predicates[functor];
    if (predicate == undefined)
        return false;
    if (predicate.foreign)
        Errors.permissionError(Constants.accessAtom, Constants.privateProcedureAtom, Utils.predicate_indicator(head));
    var clauses = predicate.clauses;
    var index = this.foreign || 0;
    if (index > clauses.length)
        return false;
    if (index + 1 < clauses.length)
        this.create_choicepoint(index+1);
    var clause = clauses[index];
    if (TAGOF(clause) == CompoundTag && FUNCTOROF(clause) == Constants.clauseFunctor)
        return this.unify(ARGOF(clause, 0), head) && this.unify(ARGOF(clause, 1), body);
    else
        return this.unify(clause, head) && this.unify(Constants.trueAtom, body);
}
// 8.8.2 current_predicate/2
module.exports.current_predicate = function(indicator)
{
    if (TAGOF(indicator) != VariableTag)
    {
        if (!(TAGOF(indicator) == CompoundTag && FUNCTOROF(indicator) == Constants.predicateIndicatorFunctor))
            Errors.typeError(Constants.predicateIndicatorAtom, indicator);
        if ((TAGOF(ARGOF(indicator, 0)) != VariableTag) && !(TAGOF(ARGOF(indicator, 0)) == ConstantTag && CTable.get(ARGOF(indicator, 0)) instanceof AtomTerm))
            Errors.typeError(Constants.predicateIndicatorAtom, indicator);
        if (!(TAGOF(ARGOF(indicator, 1) == VariableTag)) && !(TAGOF(ARGOF(indicator, 1)) == ConstantTag && CTable.get(ARGOF(indicator, 1)) instanceof IntegerTerm))
            Errors.typeError(Constants.predicateIndicatorAtom, indicator);

    }
    // Now indicator is either unbound or bound to /(A,B)
    // FIXME: Currently only examines the current module
    // FIXME: Should also allow Module:Indicator!
    var index = this.foreign || 0;
    var keys = Object.keys(this.currentModule.predicates);
    if (index > keys.length)
        return false;
    if (index + 1 < keys.length)
        this.create_choicepoint(index+1);
    var functor = CTable.get(this.currentModule.predicates[keys[index]].functor);
    return this.unify(indicator, CompoundTerm.create(Constants.predicateIndicatorFunctor, [functor.name,
                                                                                           IntegerTerm.get(functor.arity)]));
}
// 8.9.1
module.exports.asserta = function(term)
{
    // FIXME: Does not take modules into account
    this.currentModule.asserta(this.copyTerm(term));
}
// 8.9.2
module.exports.assertz = function(term)
{
    // FIXME: Does not take modules into account
    this.currentModule.assertz(this.copyTerm(term));
}
// 8.9.3
module.exports.retract = function(term)
{
    // FIXME: Does not take modules into account
    var functor = Utils.clause_functor(term);
    var functor_object = CTable.get(functor);
    var index = this.foreign || 0;
    var module = this.currentModule;
    if (module.predicates[functor].dynamic !== true)
        Errors.permissionError(Constants.modifyAtom, Constants.staticProcedureAtom, CompoundTerm.create(Constants.predicateIndicatorFunctor, [functor_object.name, IntegerTerm.get(functor_object.arity)]));
    if (index >= module.predicates[functor].clauses.length)
        return false;
    if (index + 1 < module.predicates[functor].clauses.length)
        this.create_choicepoint(index+1);
    //console.log("Trying to unify " + PORTRAY(term) + " with clause " + index + " which is: " + PORTRAY(module.predicates[functor].clauses[index]));

    if (this.unify(module.predicates[functor].clauses[index], term))
    {
        //console.log("Found a clause (" + index + "): " + PORTRAY(term));
        module.retractClause(functor, index);
        return true;
    }
    //console.log("Failed to unify with " + index + ": " + PORTRAY(module.predicates[functor].clauses[index]));
    // This will backtrack into the choicepoint we created just before, if the unificaiton failed
    return false;
}
// 8.9.4
module.exports.abolish = function(indicator)
{
    // FIXME: Does not take modules into account
    this.currentModule.abolish(indicator);
}
// 8.10 (findall/3, bagof/3, setof/3) are in builtin.pl (note that they might be much faster if implemented directly in Javascript)
// 8.11 is in iso_stream.js
// 8.12 is in iso_stream.js
// 8.13 is in iso_stream.js
// 8.14 is in iso_stream.js

// 8.15.1: (\+)/1 is compiled directly to opcodes
// 8.15.2 once/1 is in builtin.pl
// 8.15.3 repeat/0
module.exports.repeat = function()
{
    this.create_choicepoint(true);
    return true;
}
// 8.16.1
module.exports.atom_length = function(atom, length)
{
    if (TAGOF(atom) == VariableTag)
        Errors.instantiationError(atom)
    if (TAGOF(atom) != ConstantTag)
        Errors.typeError(Constants.atomAtom, atom)
    var atom_object = CTable.get(atom);
    if (!(atom_object instanceof AtomTerm))
        Errors.typeError(Constants.atomAtom, atom)
    if (TAGOF(length) != VariableTag && !(TAGOF(length) == ConstantTag && CTable.get(length) instanceof IntegerTerm))
        Errors.typeError(Constants.integerAtom, length)
    if ((TAGOF(length) == ConstantTag && (CTable.get(length) instanceof IntegerTerm) && CTable.get(length).value < 0))
        Errors.domainError(Constants.notLessThanZeroAtom, length)
    return this.unify(IntegerTerm.get(atom_object.value.length), length);
}
// 8.16.2
module.exports.atom_concat = function(atom1, atom2, atom12)
{
    var index = 0;
    if (this.foreign === undefined)
    {
        // First call
        if (TAGOF(atom1) == VariableTag && TAGOF(atom12) == VariableTag)
            Errors.instantiationError(atom1);
        if (TAGOF(atom2) == VariableTag && TAGOF(atom12) == VariableTag)
            Errors.instantiationError(atom2);
        if (TAGOF(atom1) != VariableTag)
            Utils.must_be_atom(atom1);
        if (TAGOF(atom2) != VariableTag)
            Utils.must_be_atom(atom2);
        if (TAGOF(atom12) != VariableTag)
            Utils.must_be_atom(atom12);
        if (TAGOF(atom1) == ConstantTag  && TAGOF(atom2) == ConstantTag)
        {
            // Deterministic case
            return this.unify(atom12, AtomTerm.get(CTable.get(atom1).value + CTable.get(atom2).value));
        }
        // Non-deterministic case
        index = 0;
    }
    else
        index = this.foreign;
    if (index == CTable.get(atom12).value.length+1)
    {
        return false;
    }
    this.create_choicepoint(index+1);
    return this.unify(atom1, AtomTerm.get(CTable.get(atom12).value.substring(0, index))) && this.unify(atom2, AtomTerm.get(CTable.get(atom12).value.substring(index)));
}
// 8.16.3
module.exports.sub_atom = function(atom, before, length, after, subatom)
{
    var index;
    Utils.must_be_atom(atom);
    if (TAGOF(subatom) != VariableTag)
        Utils.must_be_atom(subatom);
    if (TAGOF(before) != VariableTag)
        Utils.must_be_integer(before);
    if (TAGOF(length) != VariableTag)
        Utils.must_be_integer(length);
    if (TAGOF(after) != VariableTag)
        Utils.must_be_integer(after);

    var input = CTable.get(atom).value;
    if (this.foreign === undefined)
    {
        // First call
        index = {start:0, fixed_start:false, length:0, fixed_length:false, remaining:input.length, fixed_remaining:false};
        if (TAGOF(before) == ConstantTag && CTable.get(before) instanceof IntegerTerm)
        {
            index.fixed_start = true;
            index.start = CTable.get(before).value;
        }
        if (TAGOF(length) == ConstantTag && CTable.get(length) instanceof IntegerTerm)
        {
            index.fixed_length = true;
            index.length = CTable.get(length).value;
        }
        if (TAGOF(after) == ConstantTag && CTable.get(after) instanceof IntegerTerm)
        {
            index.fixed_remaining = true;
            index.remaining = CTable.get(after).value;
        }
        if (index.fixed_start && index.fixed_remaining && !index.fixed_length)
        {
            // Deterministic: Fall through to general case
            index.length = input.length-index.start-index.remaining;
            index.fixed_length = true;
        }
        if (index.fixed_remaining && index.fixed_length && !index.fixed_start)
        {
            // Deterministic: Fall through to general case
            index.start = input.length-index.length-index.remaining;
            index.fixed_start = true;
        }
        if (index.fixed_start && index.fixed_length)
        {
            // Deterministic general case.
            return this.unify(after, IntegerTerm.get(input.length-index.start-index.length)) &&
                this.unify(before, IntegerTerm.get(index.start)) &&
                this.unify(length, IntegerTerm.get(index.length)) &&
                this.unify(subatom, AtomTerm.get(input.substring(index.start, index.start+index.length)));
        }
    }
    else
    {
        // Retry case
        index = this.foreign;
        if (!index.fixed_length)
        {
            index.length++;
            if (index.start + index.length > input.length)
            {
                index.length = 0;
                if (!index.fixed_start)
                {
                    index.start++;
                    if (index.start > input.length)
                    {
                        return false;
                    }
                }
                else
                {
                    // start is fixed, so length and remaining are free
                    // but remaining is always just computed
                    return false;
                }
            }
        }
        else
        {
            // length is fixed, so start and remaining must be free
            index.start++;
            index.remaining--;
            if (index.length + index.start > input.length)
            {
                return false;
            }
        }
    }
    // Make a new choicepoint
    this.create_choicepoint(index);
    return this.unify(after, IntegerTerm.get(input.length-index.start-index.length)) &&
        this.unify(before, IntegerTerm.get(index.start)) &&
        this.unify(length, IntegerTerm.get(index.length)) &&
        this.unify(subatom, AtomTerm.get(input.substring(index.start, index.start+index.length)));
}
// 8.16.4
module.exports.atom_chars = function(atom, chars)
{
    if (TAGOF(atom) == VariableTag)
    {
        // Error if chars is not ground (instantiation error) or not a list (type_error(list)) or an element is not a one-char atom (type_error(character))
        var list = chars;
        var buffer = '';
        var head;
        while(true)
        {
            Utils.must_be_bound(list);
            if (TAGOF(list) == CompoundTag && FUNCTOROF(list) == Constants.listFunctor)
            {
                head = ARGOF(list, 0);
                Utils.must_be_bound(head);
                Utils.must_be_character(head);
                buffer += CTable.get(head).value;
                list = ARGOF(list, 1);
            }
            else if (list == Constants.emptyListAtom)
            {
                break;
            }
            else
                Errors.typeError(Constants.listAtom, chars);
        }
        return this.unify(atom, AtomTerm.get(buffer));
    }
    else if (TAGOF(atom) == ConstantTag && CTable.get(atom) instanceof AtomTerm)
    {
        var list = [];
        var buffer = CTable.get(atom).value;
        for (var i = 0; i < buffer.length; i++)
            list.push(AtomTerm.get(buffer[i]));
        return this.unify(Utils.from_list(list), chars);
    }
    Errors.typeError(Constants.atomAtom, atom);
}
// 8.16.5
module.exports.atom_codes = function(atom, codes)
{
    if (TAGOF(atom) == VariableTag)
    {
        var head = codes;
        var buffer = '';
        Utils.must_be_bound(codes);
        while(true)
        {
            if (TAGOF(head) == CompoundTag && FUNCTOROF(head) == Constants.listFunctor)
            {
                var h = ARGOF(head, 0);
                if (TAGOF(h) == ConstantTag && CTable.get(h) instanceof IntegerTerm && CTable.get(h).value >= 0)
                    buffer += String.fromCharCode(CTable.get(h).value);
                else
                    Errors.representationError(Constants.characterCodeAtom, h);
                head = ARGOF(head, 1);
            }
            else if (head == Constants.emptyListAtom)
            {
                break;
            }
            else
                Errors.typeError(Constants.listAtom, codes);
        }
        return this.unify(atom, AtomTerm.get(buffer));
    }
    else if (TAGOF(atom) == ConstantTag && CTable.get(atom) instanceof AtomTerm)
    {
        var list = [];
        var buffer = CTable.get(atom).value;
        for (var i = 0; i < buffer.length; i++)
            list.push(IntegerTerm.get(buffer.charCodeAt(i)));
        return this.unify(Utils.from_list(list), codes);
    }
    Errors.typeError(Constants.atomAtom, atom);
}
// 8.16.6
module.exports.char_code = function(c, code)
{
    if ((TAGOF(code) == VariableTag) && (TAGOF(c) == VariableTag))
    {
        Errors.instantiationError(code);
    }
    else if (TAGOF(c) == ConstantTag && CTable.get(c) instanceof AtomTerm)
    {
        if (TAGOF(code) != VariableTag)
            Utils.must_be_positive_integer(code);
        var c_obj = CTable.get(c);
        if (c_obj.value.length != 1)
            Errors.typeError(Constants.characterAtom, c);
        if ((TAGOF(code) == VariableTag) || (TAGOF(code) == ConstantTag && CTable.get(code) instanceof IntegerTerm))
            return this.unify(IntegerTerm.get(c_obj.value.charCodeAt(0)), code);
        Errors.representationError(Constants.characterCodeAtom, code);
    }
    Utils.must_be_bound(code);
    Utils.must_be_integer(code);
    var code_obj = CTable.get(code);
    if (code_obj.value < 0)
        Errors.representationError(Constants.characterCodeAtom, code);
    return this.unify(AtomTerm.get(String.fromCharCode(code_obj.value)), c);
}
// 8.16.7
module.exports.number_chars = function(number, chars)
{
    if (TAGOF(chars) != VariableTag)
    {
        // We have to try this first since for a given number there could be many possible values for chars. Consider 3E+0, 3.0 etc
        var list = chars;
        var head;
        var string = "";
        while (TAGOF(list) == CompoundTag && FUNCTOROF(list) == Constants.listFunctor)
        {
            head = ARGOF(list, 0);
            Utils.must_be_character(head);
            string += CTable.get(head).value;
            list = ARGOF(list, 1);
        }
        if (list != Constants.emptyListAtom)
            Errors.typeError(Constants.listAtom, chars);
        return this.unify(number, Parser.tokenToNumericTerm(string));

    }
    if (TAGOF(number) == VariableTag)
    {
        // Error if chars is not ground (instantiation error) or not a list (type_error(list)) or an element is not a one-char atom (type_error(character))
        Utils.must_be_bound(chars);
        var head = chars;
        var buffer = '';
        while(true)
        {
            if (TAGOF(head) == CompoundTag && FUNCTOROF(head) == Constants.listFunctor)
            {
                var h = ARGOF(head, 0);
                if (TAGOF(h) == ConstantTag && CTable.get(h) instanceof AtomTerm && CTable.get(h).value.length == 1)
                    buffer += CTable.get(h).value;
                else
                    Errors.typeError(Constants.characterAtom, h);
                head = ARGOF(head, 1);
            }
            else if (head == Constants.emptyListAtom)
            {
                break;
            }
            else
                Errors.typeError(Constants.listAtom, chars);
        }
        var token = Parser.numberToken(buffer);
        if (token == null)
            Errors.syntaxError(Constants.illegalNumberAtom);
        return this.unify(number, Parser.numberToken(buffer));
    }
    else if (TAGOF(number) == ConstantTag)
    {
        var n = CTable.get(number);
        if ((n instanceof IntegerTerm) || (n instanceof FloatTerm))
        {
            var string = String(n.value);
            if (n instanceof FloatTerm && parseInt(n.value) == n.value)
                string += ".0";
            var list = [];
            for (var i = 0; i < string.length; i++)
                list.push(AtomTerm.get(string[i]));
            return this.unify(Utils.from_list(list), chars);
        }
    }
    Errors.typeError(Constants.numberAtom, number);
}
// 8.16.8
module.exports.number_codes = function(number, codes)
{
    if (TAGOF(codes) != VariableTag)
    {
        // We have to try this first since for a given number there could be many possible values for codes. Consider 3E+0, 3.0 etc
        var list = codes;
        var head;
        var string = "";
        while (TAGOF(list) == CompoundTag && FUNCTOROF(list) == Constants.listFunctor)
        {
            head = ARGOF(list, 0);
            Utils.must_be_character_code(head);
            string += String.fromCharCode(CTable.get(head).value);
            list = ARGOF(list, 1);
        }
        if (list != Constants.emptyListAtom)
            Errors.typeError(Constants.listAtom, codes);
        return this.unify(number, Parser.tokenToNumericTerm(string));
    }
    if (TAGOF(number) == VariableTag)
    {
        var list = codes;
        var buffer = '';
        Utils.must_be_bound(codes);
        while(true)
        {
            if (TAGOF(list) == CompoundTag && FUNCTOROF(list) == Constants.listFunctor)
            {
                var head = ARGOF(list, 0);
                if (TAGOF(head) == ConstantTag && CTable.get(head) instanceof IntegerTerm)
                    buffer += String.fromCharCode(CTable.get(head).value);
                else
                    Errors.representationError(Constants.characterCodeAtom, head);
                list = ARGOF(list, 1);
            }
            else if (list == Constants.emptyListAtom)
            {
                break;
            }
            else
                Errors.typeError(Constants.listAtom, codes);
        }
        var token = Parser.numberToken(buffer);
        if (token == null)
            Errors.syntaxError(Constants.illegalNumberAtom);
        return this.unify(number, Parser.numberToken(buffer));
    }
    else if (TAGOF(number) == ConstantTag)
    {
        var n = CTable.get(number);
        if ((n instanceof IntegerTerm) || (n instanceof FloatTerm))
        {
            var string = String(n.value);
            if (n instanceof FloatTerm && parseInt(n.value) == n.value)
                string += ".0";
            var list = [];
            for (var i = 0; i < string.length; i++)
                list.push(IntegerTerm.get(string.charCodeAt(i)));
            return this.unify(Utils.from_list(list), codes);
        }
    }
    Errors.typeError(Constants.numberAtom, number);
}
// 8.17.1
module.exports.set_prolog_flag = function(flag, value)
{
    Utils.must_be_bound(flag);
    Utils.must_be_bound(value);
    Utils.must_be_atom(flag);
    for (var i = 0; i < PrologFlag.flags.length; i++)
    {
        if (PrologFlag.flags[i].name == CTable.get(flag).value)
        {
            PrologFlag.flags[i].fn(true, value);
            return true;
        }
    }
    Errors.domainError(Constants.prologFlagAtom, flag);
}
// 8.17.2
module.exports.current_prolog_flag = function(flag, value)
{
    if (TAGOF(flag) == VariableTag)
    {
        // Search case
        var index;
        if (this.foreign === undefined)
            index = 0;
        else
            index = this.foreign;
        if (index >= PrologFlag.flags.length)
            return false;
        if (index+1 < PrologFlag.flags.length)
            this.create_choicepoint(index+1);
        return this.unify(flag, AtomTerm.get(PrologFlag.flags[index].name)) && this.unify(value, PrologFlag.flags[index].fn(false, null));
    }
    else if (TAGOF(flag) == ConstantTag && CTable.get(flag) instanceof AtomTerm)
    {
        // Lookup case
        var f = CTable.get(flag);
        for (var i = 0; i < PrologFlag.flags.length; i++)
        {
            if (PrologFlag.flags[i].name == f.value)
            {
                return this.unify(value, PrologFlag.flags[i].fn(false, null));
            }
        }
        return Errors.domainError(Constants.prologFlagAtom, flag);
    }
    else
        Errors.typeError(Constants.atomAtom, flag);
}
// 8.17.3
module.exports.halt = [function()
                       {
                           this.halt(0);
                       },
// 8.17.4
                       function(a)
                       {
                           Utils.must_be_bound(a);
                           Utils.must_be_integer(a);
                           this.halt(a.value);
                       }];

