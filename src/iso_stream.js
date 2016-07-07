var BlobTerm = require('./blob_term');
var AtomTerm = require('./atom_term');
var IntegerTerm = require('./integer_term');
var Options = require('./options');
var Stream = require('./stream');
var Term = require('./term');
var Errors = require('./errors');
var Constants = require('./constants');
var Parser = require('./parser');
var TermWriter = require('./term_writer');
var fs = require('fs');

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

// FIXME: 7.10.2.13 requires us to support the following: file_name/1, mode/1, input/0, output/0, alias/1, position/1, end_of_stream/1 eof_action/1, reposition/1, type/1
var stream_properties = [get_stream_position];


// fs doesnt support seek() or tell(), but readSync and writeSync include position arguments. If we keep track of everything ourselves, we should be OK
function fsRead(stream, count, buffer)
{
    // FIXME: This is pretty inefficient. I should probably just use node buffers everywhere
    var b = new Buffer(count);
    var bytes = fs.readSync(stream.data.fd, b, 0, count, stream.data.position)
    if (bytes != -1)
    {
        for (var i = 0; i < bytes; i++)
            buffer[i] = b.readUInt8(i);
        stream.data.position += bytes;
    }
    return bytes;
}

function fsWrite(stream, count, buffer)
{
    // FIXME: This is pretty inefficient. I should probably just use node buffers everywhere
    var b = new Buffer(count);
    for (var i = 0; i < count; i++)
        b.writeUInt8(buffer[i], i);
    var bytes = fs.writeSync(stream.data.fd, b, 0, count, stream.data.position);
    if (bytes != -1)
        stream.data.position += bytes;
    return bytes;
}

function fsSeek(stream, position)
{
    stream.data.position = position;
}

function fsClose(stream)
{
    return fs.closeSync(stream.data.fd);
}

function fsTell(stream)
{
    return stream.data.position;
}

// path and mode are Javascript strings here, and options is a Javascript object
function fsOpen(path, mode, options)
{
    if (mode == "read")
    {
        return new Stream(fsRead, null, fsSeek, fsClose, fsTell, {fd: fs.openSync(path, 'r'),
                                                                  position: 0});
    }
    else if (mode == "write")
    {
        return new Stream(null, fsWrite, fsSeek, fsClose, fsTell, {fd: fs.openSync(path, 'w'),
                                                                   position: 0});
    }
    else if (mode == "append")
    {
        return new Stream(null, fsWrite, fsSeek, fsClose, fsTell, {fd: fs.openSync(path, 'a'),
                                                                   position: 0});
    }

}


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
        return module.exports.open[1].bind(this, file, mode, stream, Constants.emptyListAtom)();
    },
    function(file, mode, stream, options)
    {
        // FIXME: Options we must support for open/4 are given in 7.10.2.11
        Term.must_be_atom(file);
        Term.must_be_atom(mode);
        if (mode.value != "read" && mode.value != "write" && mode.value != "append") // These are the three IO modes required in 7.10.1.1
            Errors.domainError(Constants.ioModeAtom, mode);
        return this.unify(stream, new BlobTerm("stream", fsOpen(file.value, mode.value, Options.parseOptions(options, Constants.streamOptionAtom))));
    }];

// 8.11.6
module.exports.close = [
    function(stream)
    {
        return module.exports.close[1].bind(this, stream, Constants.emptyListAtom)();
    },
    function(stream, options)
    {
        stream = get_stream(stream);
        if (stream.write != null)
            stream.flush();
        if (stream.close != null)
        {
            // FIXME: If options contains force(true) then ignore errors in close(), and in fact not close the stream at all if we failed to flush. See 7.10.2.12
            return stream.close(stream);
        }
        // FIXME: If we have just closed current_input or current_output then we must set those back to stdin or stdout according to 7.10.2.4
        return false;
    }];

// 8.11.7
module.exports.flush_output = [
    function()
    {
        return module.exports.flush_output[1].bind(this, this.streams.current_output.term)();
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
        return module.exports.at_end_of_stream[1].bind(this, this.streams.current_output.term)();
    },
    function(stream)
    {
        return get_stream(stream).peekch() != -1;
    }];

// 8.11.9
module.exports.set_stream_position = function(stream, position)
{
    Term.must_be_integer(position);
    stream = get_stream(stream);
    if (stream.seek == null)
        Errors.permissionError(Constants.repositionAtom, Constants.streamAtom, stream.term);
    return stream.seek(stream, position.value);
}

// 8.12.1
module.exports.get_char = [
    function(c)
    {
        return module.exports.get_char[1].bind(this, this.streams.current_input.term, c)();
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
        return module.exports.get_code[1].bind(this, this.streams.currentintput.term, c)();
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
        return module.exports.peek_char[1].bind(this, this.streams.current_input.term, c)();
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
        return module.exports.peek_code[1].bind(this, this.streams.current_input.term, c)();
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
        return module.exports.put_char[1].bind(this, this.streams.current_output.term, c)();
    },
    function(stream, c)
    {
        Term.must_be_character(c);
        stream = get_stream(stream);
        stream.putch(c.value.charCodeAt(0));
        return true;
    }];
module.exports.put_code = [
    function(c)
    {
        return module.exports.put_code[1].bind(this, this.streams.current_output.term, c)();
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
        return module.exports.get_byte[1].bind(this, this.streams.current_input.term, c)();
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
        return module.exports.peek_byte[1].bind(this, this.streams.current_input.term, c)();
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
        return module.exports.put_byte[1].bind(this, this.streams.current_output.term, c)();
    },
    function(stream, c)
    {
        Term.must_be_integer(c);
        stream = get_stream(stream);
        stream.putb(c.value);
        return true;
    }];
// 8.14.1 read_term/3, read_term/2, read/1, read/2
module.exports.read_term = [
    function(term, options)
    {
        return this.unify(term, Parser.readTerm(this.streams.current_input, Options.parseOptions(options, Constants.readOption)));
    },
    function(stream, term, options)
    {
        return this.unify(term, Parser.readTerm(get_stream(stream), Options.parseOptions(options, Constants.readOption)));
    }];
module.exports.read = [
    function(term)
    {
        return this.unify(term, Parser.readTerm(this.streams.current_input, {}));
    },
    function(stream, term)
    {
        return this.unify(term, Parser.readTerm(get_stream(stream), {}));
    }];

// 8.14.2 write_term/3, write_term/2, write/1, write/2, writeq/1, writeq/2, write_canonical/1, write_canonical/2
module.exports.write_term = [
    function(term, options)
    {
        return TermWriter.writeTerm(this.streams.current_output, term, Options.parseOptions(options, Constants.writeOptionAtom));
    },
    function(stream, term, options)
    {
        return TermWriter.writeTerm(get_stream(stream), term, Options.parseOptions(options, Constants.writeOptionAtom));
    }];
module.exports.write = [
    function(term)
    {
        return TermWriter.writeTerm(this.streams.current_output, term, {numbervars: true});
    },
    function(stream, term)
    {
        return TermWriter.writeTerm(get_stream(stream), term, {numbervars: true});
    }];
module.exports.writeq = [
    function(term)
    {
        return TermWriter.writeTerm(this.streams.current_output, term, {numbervars: true, quoted:true});
    },
    function(stream, term)
    {
        return TermWriter.writeTerm(get_stream(stream), term, {numbervars: true, quoted:true});
    }];
module.exports.write_canonical = [
    function(term)
    {
        return TermWriter.writeTerm(this.streams.current_output, term, {ignore_ops: true, quoted:true});
    },
    function(stream, term)
    {
        return TermWriter.writeTerm(get_stream(stream), term, {ignore_ops: true, quoted:true});
    }];

// 8.14.3 op/3
module.exports.op = function(priority, fixity, op)
{
    throw new Error("not implemented");
}

// 8.14.4 current_op/3
module.exports.current_op = function(priority, fixity, op)
{
    throw new Error("not implemented");
}

// 8.14.5 char_conversion/2
module.exports.char_conversion = function(in_char, out_char)
{
    throw new Error("not implemented");
}

// 8.14.6 current_char_conversion/2
module.exports.current_char_conversion = function(in_char, out_char)
{
    throw new Error("not implemented");
}
