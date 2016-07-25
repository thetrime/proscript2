"use strict";
exports=module.exports;

var BlobTerm = require('./blob_term');
var AtomTerm = require('./atom_term');
var IntegerTerm = require('./integer_term');
var Options = require('./options');
var Stream = require('./stream');
var Utils = require('./utils');
var CompoundTerm = require('./compound_term');
var Errors = require('./errors');
var Constants = require('./constants');
var Parser = require('./parser');
var TermWriter = require('./term_writer');
var Operators = require('./operators');
var CharConversionTable = require('./char_conversion');
var fs = require('fs');
var CTable = require('./ctable');

function get_stream(s)
{
    Utils.must_be_bound(s);
    if (TAGOF(s) == ConstantTag)
    {
        s = CTable.get(s);
        if (s instanceof BlobTerm && s.type == "stream")
        {
            return s.value;
        }
        else if (s instanceof AtomTerm)
        {
            // Aliases (not yet implemented)
            Errors.domainError(Constants.streamOrAliasAtom, s);
        }
    }
    Errors.domainError(Constants.streamOrAliasAtom, s);
}

function get_stream_position(stream, property)
{
    if (stream.tell != null)
        return this.unify(CompoundTerm.create(Constants.positionAtom, [IntegerTerm.get(stream.tell(stream) - stream.buffer.length)]), property);
    return false;
}

// FIXME: 7.10.2.13 requires us to support the following: file_name/1, mode/1, input/0, output/0, alias/1, position/1, end_of_stream/1 eof_action/1, reposition/1, type/1
var stream_properties = [get_stream_position];


// fs doesnt support seek() or tell(), but readSync and writeSync include position arguments. If we keep track of everything ourselves, we should be OK
function fsRead(stream, offset, length, buffer)
{
    var bytes = fs.readSync(stream.data.fd, buffer, offset, length, stream.data.position)
    if (bytes != -1)
        stream.data.position += bytes;
    return bytes;
}

function fsWrite(stream, offset, length, buffer)
{
    var bytes = fs.writeSync(stream.data.fd, buffer, offset, length, stream.data.position);
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
        Utils.must_be_atom(file);
        Utils.must_be_atom(mode);
        mode = CTable.get(mode);
        file = CTable.get(file);
        if (mode.value != "read" && mode.value != "write" && mode.value != "append") // These are the three IO modes required in 7.10.1.1
            Errors.domainError(Constants.ioModeAtom, mode);
        return this.unify(stream, BlobTerm.get("stream", fsOpen(file.value, mode.value, Options.parseOptions(options, Constants.streamOptionAtom))));
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
    Utils.must_be_integer(position);
    stream = get_stream(stream);
    if (stream.seek == null)
        Errors.permissionError(Constants.repositionAtom, Constants.streamAtom, stream.term);
    return stream.seek(stream, CTable.get(position).value);
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
        return this.unify(c, AtomTerm.get(String.fromCharCode(t)));
    }];
module.exports.get_code = [
    function(c)
    {
        return module.exports.get_code[1].bind(this, this.streams.currentintput.term, c)();
    },
    function(stream, c)
    {
        stream = get_stream(stream);
        return this.unify(c, IntegerTerm.get(stream.getch()));
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
        return this.unify(c, AtomTerm.get(String.fromCharCode(t)));
    }];
module.exports.peek_code = [
    function(c)
    {
        return module.exports.peek_code[1].bind(this, this.streams.current_input.term, c)();
    },
    function(stream, c)
    {
        stream = get_stream(stream);
        return this.unify(c, IntegerTerm.get(stream.peekch()));
    }];

// 8.12.3
module.exports.put_char = [
    function(c)
    {
        return module.exports.put_char[1].bind(this, this.streams.current_output.term, c)();
    },
    function(stream, c)
    {
        Utils.must_be_character(c);
        stream = get_stream(stream);
        stream.putch(CTable.get(c).value.charCodeAt(0));
        return true;
    }];
module.exports.put_code = [
    function(c)
    {
        return module.exports.put_code[1].bind(this, this.streams.current_output.term, c)();
    },
    function(stream, c)
    {
        Utils.must_be_integer(c);
        stream = get_stream(stream);
        stream.putch(CTable.get(c).value);
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
        return this.unify(c, IntegerTerm.get(stream.getb()));
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
        return this.unify(c, IntegerTerm.get(stream.peekb()));
    }];
// 8.13.3
module.exports.put_byte = [
    function(c)
    {
        return module.exports.put_byte[1].bind(this, this.streams.current_output.term, c)();
    },
    function(stream, c)
    {
        Utils.must_be_integer(c);
        stream = get_stream(stream);
        stream.putb(CTable.get(c).value);
        return true;
    }];
// 8.14.1 read_term/3, read_term/2, read/1, read/2
module.exports.read_term = [
    function(term, options)
    {
        return this.unify(term, Parser.readTerm(this.streams.current_input, Options.parseOptions(options, Constants.readOptionAtom)));
    },
    function(stream, term, options)
    {
        return this.unify(term, Parser.readTerm(get_stream(stream), Options.parseOptions(options, Constants.readOptionAtom)));
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
    // FIXME: Should support a list as op, apparently
    Utils.must_be_atom(fixity);
    Utils.must_be_atom(op);
    Utils.must_be_integer(priority);
    var new_op;
    priority = CTable.get(priority);
    op = CTable.get(op);
    fixity = CTable.get(fixity);
    if (priority.value == 0)
        new_op = undefined;
    else if (priority.value > 1200)
        Errors.domainError(Constants.operatorPriorityAtom, priority);
    else if (priority.value < 0)
        Errors.domainError(Constants.operatorPriorityAtom, priority);
    else
        new_op = {precedence: priority.value, fixity: fixity.value};
    if (op.value == ",")
        Errors.permissionError(Constants.modifyAtom, Constants.operatorAtom, op);
    var slot = undefined;
    switch(fixity.value)
    {
        case "fx":
        case "fy": slot = "prefix"; break;
        case "xfx":
        case "xfy":
        case "yfx": slot = "infix"; break;
        case "xf":
        case "yf": slot = "postfix"; break;
        default: Errors.domainError(Constants.operatorSpecifierAtom, fixity);
    }
    // FIXME: Ensure we do not create a prefix and an infix op with the same name
    if (Operators[op.value] === undefined && new_op != undefined)
        Operators[op.value] = {};
    Operators[op.value][slot] = new_op;
    return true;
}

// 8.14.4 current_op/3
module.exports.current_op = function(priority, fixity, op)
{
    var index = this.foreign || 0;
    var op_count = 0;
    var next_op = undefined;
    var op_name;
    // This is a little inefficient, but it is better than operators are optimised for parsing/writing than enumerating
    for (var key in Operators)
    {
        if (Operators[key].prefix !== undefined)
        {
            if (op_count == index)
            {
                next_op = Operators[key].prefix;
                op_name = key;
            }
            op_count++;
        }
        if (Operators[key].infix !== undefined)
        {
            if (op_count == index)
            {
                next_op = Operators[key].infix;
                op_name = key;
            }
            op_count++;
        }
        if (Operators[key].postfix !== undefined)
        {
            if (op_count == index)
            {
                next_op = Operators[key].postfix;
                op_name = key;
            }
            op_count++;
        }

    }
    if (index > op_count || next_op === undefined)
        return false;
    if (index+1 < op_count)
        this.create_choicepoint(index+1);
    return this.unify(priority, IntegerTerm.get(next_op.precedence)) && this.unify(fixity, AtomTerm.get(next_op.fixity)) && this.unify(op, AtomTerm.get(op_name));
}

// 8.14.5 char_conversion/2
module.exports.char_conversion = function(in_char, out_char)
{
    Utils.must_be_character(in_char);
    Utils.must_be_character(out_char);
    CharConversionTable[CTable.get(in_char).value] = CTable.get(out_char).value;
}

// 8.14.6 current_char_conversion/2
module.exports.current_char_conversion = function(in_char, out_char)
{
    if (TAGOF(in_char) == ConstantTag && CTable.get(in_char) instanceof AtomTerm)
    {
        if (CharConversionTable[CTable.get(in_char).value] === undefined)
            return this.unify(out_char, in_char);
        else
            return this.unify(out_char, AtomTerm.get(CharConversionTable[in_char.value]));
    }
    else if (TAGOF(out_char) == ConstantTag && CTable.get(out_char) instanceof AtomTerm)
    {
        // This may or may not be deterministic
        // FIXME: Does not return identity mapping
        out_char = CTable.get(out_char);
        if (this.foreign === undefined)
        {
            var needle = out_char.value;
            var out_bindings = [];
            for (var key in CharConversionTable)
            {
                if (CharConversionTable[key] == needle)
                    out_bindings.push(key);
            }
            if (out_bindings.length == 0)
                return false;
        }
        var bindings = this.foreign;
        var binding = bindings.pop();
        if (bindings.length > 0)
            this.create_choicepoint(bindings);
        return this.unify(in_char, AtomTerm.get(binding));
    }
    else if (TAGOF(in_char) == VariableTag && TAGOF(out_char) == VariableTag)
    {
        // FIXME: Does not return identity mapping
        if (this.foreign === undefined)
        {
            var needle = out_char.value;
            var out_bindings = [];
            for (var key in CharConversionTable)
            {
                if (CharConversionTable[key] == needle)
                    out_bindings.push(key);
            }
            if (out_bindings.length == 0)
                return false;
        }
        var bindings = this.foreign;
        var binding = bindings.pop();
        if (bindings.length > 0)
            this.create_choicepoint(bindings);
        return this.unify(in_char, AtomTerm.get(binding)) && this.unify(out_char, AtomTerm.get(CharConversionTable[binding]));
    }
}
