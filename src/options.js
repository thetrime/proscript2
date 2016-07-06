var Constants = require('./constants');
var Constants = require('./constants.js');
var CompoundTerm = require('./compound_term.js');
var AtomTerm = require('./atom_term.js');
var VariableTerm = require('./variable_term.js');
var IntegerTerm = require('./integer_term.js');
var FloatTerm = require('./float_term.js');

module.exports.parseOptions = function(t, domain)
{
    var list = t;
    var options = {};
    while (list instanceof CompoundTerm && list.functor.equals(Constants.listFunctor))
    {
        var head = list.args[0];
        list = list.args[1];
        if (head instanceof VariableTerm)
            Errors.instantiationError(head);
        // FIXME: In many cases we are supposed to check that head is in some domain
        if (head instanceof AtomTerm)
            options[head.value] = true;
        else if (head instanceof CompoundTerm)
        {
            if (head.functor.arity == 1)
            {
                if (head.args[0] instanceof AtomTerm)
                {
                    if (head.args[0].value == "true")
                        options[head.functor.name.value] = true;
                    else if (head.args[0].value == "false")
                        options[head.functor.name.value] = false;
                    else
                        options[head.functor.name.value] = head.args[0].value;
                }
                else if (head.args[0] instanceof IntegerTerm)
                    options[head.functor.name.value] = head.args[0].value;
                Errors.domainError(domain, head);
            }
            Errors.domainError(domain, head);
        }
        else
            Errors.domainError(domain, head);
    }
    if (list instanceof VariableTerm)
        Errors.instantiationError(list);
    if (!list.equals(Constants.emptyListAtom))
        Errors.typeError(Constants.listAtom, t);
    return options;
}
