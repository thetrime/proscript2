"use strict";

var ArrayUtils = require('./array_utils.js');
var Constants = require('./constants.js');
var CompoundTerm = require('./compound_term.js');
var AtomTerm = require('./atom_term.js');
var VariableTerm = require('./variable_term.js');
var IntegerTerm = require('./integer_term.js');
var FloatTerm = require('./float_term.js');
var Parser = require('./parser.js');
var PrologFlag = require('./prolog_flag.js');
var Term = require('./term.js');

// Convenience function that returns a Prolog term corresponding to the given list of prolog terms.
function term_from_list(list, tail)
{
    var result = tail || Constants.emptyListAtom;
    for (var i = list.length-1; i >= 0; i--)
        result = new CompoundTerm(Constants.listFunctor, [list[i], result]);
    return result;
}

// Function that returns a Javascript list of the terms in the prolog list. Raises an error if the list is not a proper list
function list_from_term(term)
{
    var list = [];
    while (term instanceof CompoundTerm && term.functor.equals(Constants.listFunctor))
    {
        list.push(term.args[0]);
        term = term.args[1];
    }
    if (!term.equals(Constants.emptyListAtom))
        throw new Error("Not a proper list"); // FIXME: Should throw a prolog error really
}




function acyclic_term(t)
{
    var visited = [];
    var stack = [t.dereference()];
    console.log("Checking whether term is cyclic...");
    while (stack.length != 0)
    {
        var arg = stack.pop();
        if (arg instanceof VariableTerm)
        {
            var needle = arg.dereference();
            for (var i = 0; i < visited.length; i++)
            {
                if (visited[i] === needle)
                {
                    return false;
                }
            }
        }
        else if (arg instanceof CompoundTerm)
        {
            visited.push(arg);
            for (var i = 0; i < arg.args.length; i++)
                stack.push(arg.args[i]);
        }
    }
    return true;
}

// Computes a-b, so that if a is bigger than b, the result is positive
function term_difference(a, b)
{
    if (a.equals(b))
        return 0;
    if (a instanceof VariableTerm)
    {
        if (b instanceof VariableTerm)
            return a.index - b.index;
        return 1;
    }
    if (a instanceof FloatTerm)
    {
        if (b instanceof VariableTerm)
            return -1;
        else if (b instanceof FloatTerm)
            return a.value - b.value;
        return 1;
    }
    if (a instanceof IntegerTerm)
    {
        if ((b instanceof VariableTerm) || (b instanceof FloatTerm))
            return -1;
        else if (b instanceof IntegerTerm)
            return a.value - b.value;
        return 1;
    }
    if (a instanceof AtomTerm)
    {
        if ((b instanceof VariableTerm) || (b instanceof FloatTerm) || (b instanceof IntegerTerm))
            return -1;
        else if (b instanceof AtomTerm)
            return (a.value > b.value)?1:-1;
        return 1;
    }
    if (a instanceof CompoundTerm)
    {
        if ((b instanceof VariableTerm) || (b instanceof FloatTerm) || (b instanceof IntegerTerm) || (b instanceof AtomTerm))
            return -1;
        if (b instanceof CompoundTerm)
        {
            if (a.functor.arity != b.functor.arity)
                return a.functor.arity - b.functor.arity;
            if (a.functor.name.value != b.functor.name.value)
                return a.functor.name.value>b.functor.name.value?1:-1;
            for (var i = 0; i < a.functor.arity; i++)
            {
                var d = term_difference(a.args[i], b.args[i]);
                if (d != 0)
                    return d;
            }
            return 0;
        }
        return 1;
    }
    // FIXME: Other types here
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
    var b = this.new_choicepoint();
    var result = !unify(a, b);
    this.backtrack(b);
    return result
}
// 8.3.1 var/1
module.exports["var"] = function(a)
{
    return (a instanceof VariableTerm);
}
// 8.3.2 atom/1
module.exports["atom"] = function(a)
{
    return (a instanceof AtomTerm);
}
// 8.3.3 integer/1
module.exports["integer"] = function(a)
{
    return (a instanceof IntegerTerm);
}
// 8.3.4 float/1
module.exports["float"] = function(a)
{
    return (a instanceof FloatTerm);
}
// 8.3.5 atomic/1
module.exports.atomic = function(a)
{
    return ((a instanceof AtomTerm) ||
            (a instanceof IntegerTerm) ||
            (a instanceof FloatTerm));
    // FIXME: Bignum support
}
// 8.3.6 compound/1
module.exports.compound = function(a)
{
    return (a instanceof CompoundTerm);
}
// 8.3.7 nonvar/1
module.exports.nonvar = function(a)
{
    return !(a instanceof VariableTerm);
}
// 8.3.8 number/1
module.exports.number = function(a)
{
    return ((a instanceof IntegerTerm) ||
            (a instanceof FloatTerm));
    // FIXME: Bignum support
}
// 8.4.1 term-comparison
module.exports["@=<"] = function(a, b)
{
    return term_difference(a, b) <= 0;
}
module.exports["=="] = function(a, b)
{
    return term_difference(a, b) == 0;
}
module.exports["\\=="] = function(a, b)
{
    return term_difference(a, b) != 0;
}
module.exports["@<"] = function(a, b)
{
    return term_difference(a, b) < 0;
}
module.exports["@>"] = function(a, b)
{
    return term_difference(a, b) > 0;
}
module.exports["@>="] = function(a, b)
{
    return term_difference(a, b) >= 0;
}
// 8.5.1 functor/3
module.exports.functor = function(term, name, arity)
{
    if (term instanceof VariableTerm)
    {
        // Construct a term
        if (arity instanceof VariableTerm)
            Errors.instantiationError(arity);
        if (arity instanceof VariableTerm)
            Errors.instantiationError(arity);
        if (!(arity instanceof IntegerTerm))
            Errors.typeError(Constants.integerAtom, arity);
        var args = new Array(arity.value);
        for (var i = 0; i < args.length; i++)
            args[i] = new VariableTerm();
        console.log(name + ", " + args);
        return this.unify(term, new CompoundTerm(name, args));
    }
    if (term instanceof AtomTerm)
    {
        return this.unify(name, term) && this.unify(arity, new IntegerTerm(0));
    }
    if (term instanceof IntegerTerm)
    {
        return this.unify(name, term) && this.unify(arity, new IntegerTerm(0));
    }
    if (term instanceof CompoundTerm)
    {
        return this.unify(name, term.functor.name) && this.unify(arity, new IntegerTerm(term.functor.arity));
    }
    else
        Errors.typeError(Constants.compoundAtom, term);
}
// 8.5.2 arg/3
module.exports.arg = function(n, term, arg)
{
    // NB: ISO only requires the +,+,? mode
    if (n instanceof VariableTerm)
        Errors.instantiationError(n);
    if (term instanceof VariableTerm)
        Errors.instantiationError(term);
    if (!(n instanceof IntegerTerm))
        Errors.typeError(Constants.integerAtom, n);
    if (n.value < 0)
        Errors.domainError(Constants.notLessThanZeroAtom, n);
    if (!(term instanceof CompoundTerm))
        Errors.typeError(Constants.compoundAtom, term);
    if (term.args[n.value] === undefined)
        return false; // N is too big
    return this.unify(term.args[n.value], arg);
}
// 8.5.3 (=..)/2
module.exports["=.."] = function(term, univ)
{
    // CHECKME: More errors should be checked
    if (term instanceof AtomTerm) // FIXME: Also other atomic-type terms
        return this.unify(univ, term_from_list([term]));
    if (term instanceof CompoundTerm)
    {
        return this.unify(univ, term_from_list([term.functor.name].concat(term.args)));
    }
    if (term instanceof VariableTerm)
    {
        var list = list_from_term(univ);
        var fname = list.unshift();
        return this.unify(term, new CompoundTerm(fname, list));
    }
    else
        Errors.typeError(Constants.termAtom, term);
}
// 8.5.4 copy_term/2
module.exports.copy_term = function(term, copy)
{
    return this.unify(Kernel.copyTerm(term), copy);
}
// 8.8.1 clause/2
module.exports.clause = function(head, body)
{
    // FIXME: Assumes current module
    // FIXME: Should allow Module:Head
    var functor = Term.head_functor(head);
    var clauses = this.currentModule.predicates[functor.toString()].clauses;
    var index = this.foreign || 0;
    if (index > clauses.length)
        return false;
    if (index + 1 < clauses.length)
        this.create_choicepoint(index+1);
    var clause = clauses[index];
    if (clause instanceof CompoundTerm && clause.functor.equals(Constants.clauseFunctor))
        return this.unify(clause.args[0], head) && this.unify(clause.args[1], body);
    else
        return this.unify(clause, head) && this.unify(Constants.trueAtom, body);
}
// 8.8.2 current_predicate/2
module.exports.current_predicate = function(indicator)
{
    if (indicator instanceof CompoundTerm)
    {
        if (!indicator.functor.equals(Constants.predicateIndicatorFunctor))
            Errors.typeError(Constants.predicateIndicatorAtom, indicator);
    }
    else if (!indicator instanceof VariableTerm)
        Errors.typeError(Constants.predicateIndicatorAtom, indicator);
    // Now indicator is either unbound or bound to /(A,B)
    // FIXME: Currently only examines the current module
    // FIXME: Should also allow Module:Indicator!
    var index = this.foreign || 0;
    var keys = Object.keys(this.currentModule.predicates);
    if (index > keys.length)
        return false;
    if (index + 1 < keys.length)
        this.create_choicepoint(index+1);
    return this.unify(indicator, new CompoundTerm(Constants.predicateIndicatorFunctor, [this.currentModule.predicates[keys[index]].functor.name,
                                                                                        new IntegerTerm(this.currentModule.predicates[keys[index]].functor.arity)]));
}
// 8.9.1
module.exports.asserta = function(term)
{
    // FIXME: Does not take modules into account
    this.currentModule.asserta(term);
}
// 8.9.2
module.exports.assertz = function(term)
{
    // FIXME: Does not take modules into account
    this.currentModule.assertz(term);
}
// 8.9.3
module.exports.retract = function(term)
{
    // FIXME: Does not take modules into account
    var functor = Term.clause_functor(term);
    var index = this.foreign || 0;
    var module = this.currentModule;
    if (module.predicates[functor.toString()].dynamic !== true)
        Errors.permissionError(Constants.accessAtom, Constants.staticProcedureAtom, term);
    if (index >= module.predicates[functor.toString()].clauses.length)
        return false;
    if (index + 1 < module.predicates[functor.toString()].clauses.length)
        this.create_choicepoint(index+1);
    if (this.unify(module.predicates[functor.toString()].clauses[index], term))
    {
        module.retractClause(functor, index);
        return true;
    }
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
    if (atom instanceof VariableTerm)
        Errors.instantiationError(atom)
    if (!(atom instanceof AtomTerm))
        Errors.typeError(Constants.atomAtom, atom)
    if (!((length instanceof IntegerTerm) || length instanceof VariableTerm))
        Errors.typeError(Constants.integerAtom, length)
    if ((length instanceof IntegerTerm) && length.value < 0)
        Errors.domainError(Constants.notLessThanZeroAtom, length)
    return this.unify(new IntegerTerm(atom.value.length), length);
}
// 8.16.2
module.exports.atom_concat = function(atom1, atom2, atom12)
{
    var index = 0;
    if (this.foreign === undefined)
    {
        // First call
        if (atom1 instanceof VariableTerm && atom12 instanceof VariableTerm)
            Errors.instantiationError(atom1);
        if (atom2 instanceof VariableTerm && atom12 instanceof VariableTerm)
            Errors.instantiationError(atom2);
        if (!(atom1 instanceof VariableTerm))
            Term.must_be_atom(atom1);
        if (!(atom2 instanceof VariableTerm))
            Term.must_be_atom(atom2);
        if (!(atom12 instanceof VariableTerm))
            Term.must_be_atom(atom12);
        if (atom1 instanceof AtomTerm && atom2 instanceof AtomTerm)
        {
            // Deterministic case
            return this.unify(atom12, new AtomTerm(atom1.value + atom2.value));
        }
        // Non-deterministic case
        index = 0;
    }
    else
        index = this.foreign;
    if (index == atom12.value.length+1)
    {
        return false;
    }
    this.create_choicepoint(index+1);
    return this.unify(atom1, new AtomTerm(atom12.value.substring(0, index))) && this.unify(atom2, new AtomTerm(atom12.value.substring(index)));
}
// 8.16.3
module.exports.sub_atom = function(atom, before, length, after, subatom)
{
    var index;
    Term.must_be_bound(atom);
    Term.must_be_atom(atom);
    if (!(subatom instanceof VariableTerm))
        Term.must_be_atom(subatom);
    var input = atom.value;
    if (this.foreign === undefined)
    {
        // First call
        index = {start:0, fixed_start:false, length:0, fixed_length:false, remaining:input.length, fixed_remaining:false};
        if (before instanceof IntegerTerm)
        {
            index.fixed_start = true;
            index.start = before.value;
        }
        if (length instanceof IntegerTerm)
        {
            index.fixed_length = true;
            index.length = length.value;
        }
        if (after instanceof IntegerTerm)
        {
            index.fixed_remaining = true;
            index.remaining = after.value;
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
            return this.unify(after, new IntegerTerm(input.length-index.start-index.length)) &&
                this.unify(before, new IntegerTerm(index.start)) &&
                this.unify(length, new IntegerTerm(index.length)) &&
                this.unify(subatom, new AtomTerm(input.substring(index.start, index.start+index.length)));
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
    return this.unify(after, new IntegerTerm(input.length-index.start-index.length)) &&
        this.unify(before, new IntegerTerm(index.start)) &&
        this.unify(length, new IntegerTerm(index.length)) &&
        this.unify(subatom, new AtomTerm(input.substring(index.start, index.start+index.length)));
}
// 8.16.4
module.exports.atom_chars = function(atom, chars)
{
    if (atom instanceof VariableTerm)
    {
        // Error if chars is not ground (instantiation error) or not a list (type_error(list)) or an element is not a one-char atom (type_error(character))
        var head = chars;
        var buffer = '';
        while(true)
        {
            if (head instanceof CompoundTerm && head.functor.equals(Constants.listFunctor))
            {
                if (head.args[0] instanceof AtomTerm && head.args[0].value.length == 1)
                    buffer += head.args[0].value;
                else
                    Errors.typeError(Constants.characterAtom, head.args[0]);
                head = head.args[1];
            }
            else if (head.equals(Constants.emptyListAtom))
            {
                break;
            }
            else
                Errors.typeError(Constants.listAtom, chars);
        }
        return this.unify(atom, new AtomTerm(buffer));
    }
    else if (atom instanceof AtomTerm)
    {
        var list = [];
        for (var i = 0; i < atom.value.length; i++)
            list.push(new AtomTerm(atom.value[i]));
        return this.unify(list_from_term(list), chars);
    }
    Errors.typeError(Constants.atomAtom, atom);
}
// 8.16.5
module.exports.atom_codes = function(atom, codes)
{
    if (atom instanceof VariableTerm)
    {
        var head = codes;
        var buffer = '';
        while(true)
        {
            if (head instanceof CompoundTerm && head.functor.equals(Constants.listFunctor))
            {
                if (head.args[0] instanceof IntegerTerm)
                    buffer += String.fromCharCode(head.args[0].value);
                else
                    Errors.representationError(Constants.characterCodeAtom, head.args[0]);
                head = head.args[1];
            }
            else if (head.equals(Constants.emptyListAtom))
            {
                break;
            }
            else
                Errors.typeError(Constants.listAtom, codes);
        }
        return this.unify(atom, new AtomTerm(buffer));
    }
    else if (atom instanceof AtomTerm)
    {
        var list = [];
        for (var i = 0; i < atom.value.length; i++)
            list.push(new IntegerTerm(atom.value.charCodeAt(i)));
        return this.unify(term_from_list(list), codes);
    }
    Errors.typeError(Constants.atomAtom, atom);
}
// 8.16.6
module.exports.char_code = function(c, code)
{
    if (c instanceof AtomTerm)
    {
        if (c.value.length != 1)
            Errors.typeError(Constants.characterAtom, c);
        if ((code instanceof VariableTerm) || (code instanceof IntegerTerm))
            return this.unify(new IntegerTerm(c.value.charCodeAt(0)), code);
        Errors.representationError(Constants.characterCodeAtom, code);
    }
    else if (code instanceof IntegerTerm)
    {
        return this.unify(new AtomTerm(String.fromCharCode(code.value)), c);
    }
    else if ((code instanceof VariableTerm) && (c instanceof VariableTerm))
    {
        Errors.instantiationError(code);
    }
    Errors.typeError(Constants.characterAtom, c);
}
// 8.16.7
module.exports.number_chars = function(number, chars)
{
    if (number instanceof VariableTerm)
    {
        // Error if chars is not ground (instantiation error) or not a list (type_error(list)) or an element is not a one-char atom (type_error(character))
        var head = chars;
        var buffer = '';
        while(true)
        {
            if (head instanceof CompoundTerm && head.functor.equals(Constants.listFunctor))
            {
                console.log(head.args[0].getClass());
                if (head.args[0] instanceof AtomTerm && head.args[0].value.length == 1)
                    buffer += head.args[0].value;
                else
                    Errors.typeError(Constants.characterAtom, head.args[0]);
                head = head.args[1];
            }
            else if (head.equals(Constants.emptyListAtom))
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
    else if ((number instanceof IntegerTerm) || (number instanceof FloatTerm))
    {
        var string = String(number.value);
        var list = [];
        for (var i = 0; i < string.length; i++)
            list.push(new AtomTerm(string.value[i]));
        return this.unify(list_from_term(list), chars);
    }
    Errors.typeError(Constants.numberAtom, number);
}
// 8.16.8
module.exports.number_codes = function(number, codes)
{
    if (number instanceof VariableTerm)
    {
        var head = codes;
        var buffer = '';
        while(true)
        {
            if (head instanceof CompoundTerm && head.functor.equals(Constants.listFunctor))
            {
                if (head.args[0] instanceof IntegerTerm)
                    buffer += String.fromCharCode(head.args[0].value);
                else
                    Errors.representationError(Constants.characterCodeAtom, head.args[0]);
                head = head.args[1];
            }
            else if (head.equals(Constants.emptyListAtom))
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
    else if ((number instanceof IntegerTerm) || (number instanceof FloatTerm))
    {
        var string = String(number.value);
        var list = [];
        for (var i = 0; i < string.length; i++)
            list.push(new IntegerTerm(string.charCodeAt(i)));
        return this.unify(term_from_list(list), codes);
    }
    Errors.typeError(Constants.numberAtom, number);
}
// 8.17.1
module.exports.set_prolog_flag = function(flag, value)
{
    Term.must_be_bound(flag);
    Term.must_be_bound(value);
    Term.must_be_atom(flag);
    for (var i = 0; i < PrologFlag.flags.length; i++)
    {
        if (PrologFlag.flags[i].name == flag.value)
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
    if (flag instanceof VariableTerm)
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
        return this.unify(flag, new AtomTerm(PrologFlag.flags[index].name)) && this.unify(value, PrologFlag.flags[index].fn(false, null));
    }
    else if (flag instanceof AtomTerm)
    {
        // Lookup case
        for (var i = 0; i < PrologFlag.flags.length; i++)
        {
            if (PrologFlag.flags[i].name == flag.value)
            {
                return this.unify(value, PrologFlag.flags[i].fn(false, null));
            }
        }
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
                           Term.must_be_bound(a);
                           Term.must_be_integer(a);
                           this.halt(a.value);
                       }];

