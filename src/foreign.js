var ArrayUtils = require('./array_utils.js');

module.exports.writeln = function(arg)
{
    var bytes = ArrayUtils.toByteArray(arg.toString());
    console.log(bytes);
    return this.streams.stdout.write(this.streams.stdout, 1, bytes.length, bytes) >= 0;
}

module.exports.fail = function()
{
    return false;
}

module.exports.true = function()
{
    return true;
}

