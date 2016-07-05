var BlobTerm = require('./blob_term');
var AtomTerm = require('./atom_term');

// 8.11.1
module.exports.current_input = function(stream)
{
    return this.unify(stream, this.streams.current_input);
}
// 8.11.2
module.exports.current_output = function(stream)
{
    return this.unify(stream, this.streams.current_output);
}
// 8.11.3
module.exports.set_input = function(stream)
{
    if (stream instanceof VariableTerm)
        Errors.instantiationError(stream);
    if ((stream instanceof BlobTerm) && stream.type == "stream")
        this.streams.current_input = stream;
    Errors.typeError(Constants.streamAtom, stream);
}
// 8.11.4
module.exports.set_output = function(stream)
{
   if (stream instanceof VariableTerm)
        Errors.instantiationError(stream);
    if ((stream instanceof BlobTerm) && stream.type == "stream")
        this.streams.current_output = stream;
    Errors.typeError(Constants.streamAtom, stream);
}

// 8.11.5
module.exports.open = [
    function(file, mode, stream)
    {
        module.exports.open[1](file, mode, stream, Constants.emptyListAtom);
    },
    function(file, mode, stream, options)
    {
        throw new Error("FIXME: Not implemented");
    }];

// 8.11.6
module.exports.close = [
    function(stream)
    {
        module.exports.close[1](stream, Constants.emptyListAtom);
    },
    function(stream, options)
    {
        throw new Error("FIXME: Not implemented");
    }];

// 8.11.7
module.exports.flush_output = [
    function()
    {
        module.exports.flush_output[1](this.streams.current_output);
    },
    function(stream)
    {
        throw new Error("FIXME: Not implemented");
    }];

// 8.11.8
module.exports.stream_property = function(stream, property)
{
    throw new Error("FIXME: Not implemented");
}
module.exports.at_end_of_stream = [
    function()
    {
        module.exports.at_end_of_stream[1](this.streams.current_output);
    },
    function(stream)
    {
        throw new Error("FIXME: Not implemented");
    }];
// 8.11.9
module.exports.set_stream_position = function(stream, position)
{
    throw new Error("FIXME: Not implemented");
}

// 8.12.1
module.exports.get_char = [
    function(c)
    {
        module.exports.get_char[1](this.streams.current_input, c);
    },
    function(stream, c)
    {
        throw new Error("FIXME: Not implemented");
    }];
module.exports.get_code = [
    function(c)
    {
        module.exports.get_code[1](this.streams.currentintput, c);
    },
    function(stream, c)
    {
        throw new Error("FIXME: Not implemented");
    }];

// 8.12.2
module.exports.peek_char = [
    function(c)
    {
        module.exports.peek_char[1](this.streams.current_input, c);
    },
    function(stream, c)
    {
        throw new Error("FIXME: Not implemented");
    }];
module.exports.peek_code = [
    function(c)
    {
        module.exports.peek_code[1](this.streams.current_input, c);
    },
    function(stream, c)
    {
        throw new Error("FIXME: Not implemented");
    }];

// 8.12.3
module.exports.put_char = [
    function(c)
    {
        module.exports.put_char[1](this.streams.current_output, c);
    },
    function(stream, c)
    {
        throw new Error("FIXME: Not implemented");
    }];
module.exports.put_code = [
    function(c)
    {
        module.exports.put_code[1](this.streams.current_output, c);
    },
    function(stream, c)
    {
        throw new Error("FIXME: Not implemented");
    }];

// 8.13.1
module.exports.get_byte = [
    function(c)
    {
        module.exports.get_byte[1](this.streams.current_input, c);
    },
    function(stream, c)
    {
        throw new Error("FIXME: Not implemented");
    }];
// 8.13.2
module.exports.peek_byte = [
    function(c)
    {
        module.exports.peek_byte[1](this.streams.current_input, c);
    },
    function(stream, c)
    {
        throw new Error("FIXME: Not implemented");
    }];
// 8.13.3
module.exports.put_byte = [
    function(c)
    {
        module.exports.put_byte[1](this.streams.current_output, c);
    },
    function(stream, c)
    {
        throw new Error("FIXME: Not implemented");
    }];
