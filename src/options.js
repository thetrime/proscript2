"use strict";
exports=module.exports;

var Constants = require('./constants');
var Constants = require('./constants.js');
var CompoundTerm = require('./compound_term.js');
var AtomTerm = require('./atom_term.js');
var VariableTerm = require('./variable_term.js');
var IntegerTerm = require('./integer_term.js');
var CTable = require('./ctable.js');

module.exports.parseOptions = function(t, domain)
{
    var list = t;
    var options = {};
    while (TAGOF(list) == CompoundTag && FUNCTOROF(list) == Constants.listFunctor)
    {
        var head = ARGOF(list, 0);
        list = ARGOF(list, 1);
        if (TAGOF(head) == VariableTag)
            Errors.instantiationError(head);
        // FIXME: In many cases we are supposed to check that head is in some domain
        if (TAGOF(head) == ConstantTag && CTable.get(head) instanceof AtomTerm)
            options[CTable.get(head).value] = true;
        else if (TAGOF(head) == CompoundTag)
        {
            var functor = CTable.get(FUNCTOROF(head));
            if (functor.arity == 1)
            {
                var h0 = ARGOF(head,0);
                if (TAGOF(h0) == ConstantTag && CTable.get(h0) instanceof AtomTerm)
                {
                    h0 = CTable.get(h0);
                    if (h0.value == "true")
                        options[CTable.get(functor.name).value] = true;
                    else if (h0.value == "false")
                        options[CTable.get(functor.name).value] = false;
                    else
                        options[CTable.get(functor.name).value] = h0.value;
                }
                else if (TAGOF(h0) == ConstantTag && CTable.get(h0) instanceof IntegerTerm)
                    options[CTable.get(functor.name).value] = CTable.get(h0).value;
                Errors.domainError(domain, head);
            }
            Errors.domainError(domain, head);
        }
        else
            Errors.domainError(domain, head);
    }
    if (TAGOF(list) == VariableTag)
        Errors.instantiationError(list);
    if (list != Constants.emptyListAtom)
        Errors.typeError(Constants.listAtom, t);
    return options;
}
