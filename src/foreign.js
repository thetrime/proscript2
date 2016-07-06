// This module contains non-ISO extensions

module.exports.term_variables = function(t, vt)
{
    return this.unify(term_from_list(term_variables(t), Constants.emptyListAtom), vt);
}
