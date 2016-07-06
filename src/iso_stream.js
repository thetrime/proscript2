var BlobTerm = require('./blob_term');
var AtomTerm = require('./atom_term');

function get_stream(s)
{
    Term.must_be_bound(s);
    if (s instanceof BlobTerm)
    {
        if (s.type == "stream")
            return s.value;
    }
    else if (s instanceof AtomTerm)
    {
        // Aliases (not yet implemented)
        Errors.domainError(Constants.streamOrAliasTerm, s);
    }
    Errors.domainError(Constants.streamOrAliasTerm, s);
}

function get_stream_position(stream, property)
{
    if (stream.tell != null)
        return this.unify(new CompoundTerm(Constants.positionAtom, [new IntegerTerm(stream.tell(stream) - stream.buffer.length)]), property);
    return false;
}

var stream_properties = [get_stream_position];


// 8.11.1
module.exports.current_input = function(stream)
{
    return this.unify(stream, this.streams.current_input.term);
}
// 8.11.2
module.exports.current_output = function(stream)
{
    return this.unify(stream, this.streams.current_output.term);
}
// 8.11.3
module.exports.set_input = function(stream)
{
    this.streams.current_input = get_stream(stream);
}
// 8.11.4
module.exports.set_output = function(stream)
{
    this.streams.current_output = get_stream(stream);
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
        stream = get_stream(stream);
        if (stream.write != null)
            stream.flush();
        if (stream.close != null)
        {
            // FIXME: If options contains force(true) then ignore errors in close()
            return stream.close();
        }
        return false;
    }];

// 8.11.7
module.exports.flush_output = [
    function()
    {
        module.exports.flush_output[1](this.streams.current_output);
    },
    function(stream)
    {
        get_stream(stream).flush();
    }];

// 8.11.8
module.exports.stream_property = function(stream, property)
{
    stream = get_stream(stream);
    var index = this.foreign || 0;
    if (index > stream_properties.length)
        return false;
    if (index + 1 < stream_properties.length)
        this.create_choicepoint(index+1);
    return stream_properties[index](stream, property);
}
module.exports.at_end_of_stream = [
    function()
    {
        module.exports.at_end_of_stream[1](this.streams.current_output);
    },
    function(stream)
    {
        return get_stream(stream).peekch() != -1;
    }];
// 8.11.9
module.exports.set_stream_position = function(stream, position)
{
    stream = get_stream(stream);
}

// 8.12.1
module.exports.get_char = [
    function(c)
    {
        module.exports.get_char[1](this.streams.current_input, c);
    },
    function(stream, c)
    {
        stream = get_stream(stream);
        var t = stream.getch();
        if (t == -1)
            return this.unify(c, Constants.endOfFileAtom);
        return this.unify(c, new AtomTerm(String.fromCharCode(t)));
    }];
module.exports.get_code = [
    function(c)
    {
        module.exports.get_code[1](this.streams.currentintput, c);
    },
    function(stream, c)
    {
        stream = get_stream(stream);
        return this.unify(c, new IntegerTerm(stream.getch()));
    }];

// 8.12.2
module.exports.peek_char = [
    function(c)
    {
        module.exports.peek_char[1](this.streams.current_input, c);
    },
    function(stream, c)
    {
        stream = get_stream(stream);
        var t = stream.peekch();
        if (t == -1)
            return this.unify(c, Constants.endOfFileAtom);
        return this.unify(c, new AtomTerm(String.fromCharCode(t)));
    }];
module.exports.peek_code = [
    function(c)
    {
        module.exports.peek_code[1](this.streams.current_input, c);
    },
    function(stream, c)
    {
        stream = get_stream(stream);
        return this.unify(c, new IntegerTerm(stream.peekch()));
    }];

// 8.12.3
module.exports.put_char = [
    function(c)
    {
        module.exports.put_char[1](this.streams.current_output, c);
    },
    function(stream, c)
    {
        Term.must_be_character(c);
        stream = get_stream(stream);
        stream.putch(String.fromCharCode(c.value));
        return true;
    }];
module.exports.put_code = [
    function(c)
    {
        module.exports.put_code[1](this.streams.current_output, c);
    },
    function(stream, c)
    {
        Term.must_be_integer(c);
        stream = get_stream(stream);
        stream.putch(c.value);
        return true;
    }];

// 8.13.1
module.exports.get_byte = [
    function(c)
    {
        module.exports.get_byte[1](this.streams.current_input, c);
    },
    function(stream, c)
    {
        stream = get_stream(stream);
        return this.unify(c, new IntegerTerm(stream.getb()));
    }];
// 8.13.2
module.exports.peek_byte = [
    function(c)
    {
        module.exports.peek_byte[1](this.streams.current_input, c);
    },
    function(stream, c)
    {
        stream = get_stream(stream);
        return this.unify(c, new IntegerTerm(stream.peekb()));
    }];
// 8.13.3
module.exports.put_byte = [
    function(c)
    {
        module.exports.put_byte[1](this.streams.current_output, c);
    },
    function(stream, c)
    {
        Term.must_be_integer(c);
        stream = get_stream(stream);
        stream.putb(c.value);
        return true;
    }];
