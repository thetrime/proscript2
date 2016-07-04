// This module contains non-ISO extensions

var ArrayUtils = require('./array_utils');

module.exports.writeln = function(arg)
{
    var bytes = ArrayUtils.toByteArray(arg.toString());
    return this.streams.stdout.write(this.streams.stdout, 1, bytes.length, bytes) >= 0;
}


module.exports.term_variables = function(t, vt)
{
    return this.unify(term_from_list(term_variables(t), Constants.emptyListAtom), vt);
}