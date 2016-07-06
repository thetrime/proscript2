var BlobTerm = require('./blob_term');
var STREAM_BUFFER_SIZE = 256;

function Stream(read, write, seek, close, tell, userdata)
{
    this.read = read;
    this.write = write;
    this.seek = seek;
    this.close = close;
    this.tell = tell;
    this.data = userdata;
    this.buffer = [];
    this.buffer_size = 0;
    this.term = new BlobTerm("stream", this);
}

Stream.prototype.flush = function()
{
    if (this.write == null)
        Errors.permissionError(Constants.outputAtom, Constants.streamAtom, this.term);
    return this.buffer_size == this.write(this, 1, this.buffer_size, this.buffer);
}

Stream.prototype.get_raw_char = function()
{
    var t = this.getch();
    if (t == -1)
        return -1;
    else
        return String.fromCharCode(t);
}

Stream.prototype.peek_raw_char = function()
{
    var t = this.peekch();
    if (t == -1)
        return -1;
    else
        return String.fromCharCode(t);
}

Stream.prototype.getch = function()
{
    var b = this.getb();
    var ch;
    if (b == -1)
        return -1;
    // ASCII
    if (b <= 0x7F)
        return b;
    ch = 0; 
    // Otherwise we have to crunch the numbers
    var mask = 0x20;
    var i = 0;
    // The first byte has leading bits 1, then a 1 for every additional byte we need followed by a 0
    // After the 0 is the top 1-5 bits of the final character. This makes it quite confusing.
    for (var mask = 0x20; mask != 0; mask >>=1 )
    {        
        var next = this.getb();
        if (next == -1)
            return -1;
        ch = (ch << 6) ^ (next & 0x3f);
        if ((b & mask) == 0)
            break;
        i++;
    }
    ch ^= (b & (0xff >> (i+3))) << (6*(i+1));
    return ch;        
}

Stream.prototype.peekch = function()
// See getch for an explanation of what is going on here
{
    var b = this.peekb();
    var ch;
    if (b == -1)
        return -1;
    // ASCII
    if (b <= 0x7F)
        return b;
    ch = 0; 
    var mask = 0x20;
    var i = 0;
    for (var mask = 0x20; mask != 0; mask >>=1 )
    {        
        var next = this.buffer[i+1];
        if (next == undefined)
        {
            // This is a problem. We need to buffer more data! But we must also not lose the existing buffer since we are peeking.
            abort("Unicode break in peekch. This is a bug");
        }
        if (next == -1)
            return -1;
        ch = (ch << 6) ^ (next & 0x3f);
        if ((b & mask) == 0)
            break;
        i++;
    }
    ch ^= (b & (0xff >> (i+3))) << (6*(i+1));
    return ch; 
}

Stream.prototype.getb = function()
{
    if (this.buffer_size == 0)
    {
        //debug_msg("Buffering...");
        this.buffer_size = this.read(this, 1, STREAM_BUFFER_SIZE, this.buffer);
        //debug_msg("Buffer now contains " + this.buffer_size + " elements");
    }
    if (this.buffer_size < 0)
        return this.buffer_size;
    // FIXME: Can this STILL be 0?
    if (this.buffer_size == 0)
        return -1;
    // At this point the buffer has some data in it
    this.buffer_size--;
    return this.buffer.shift();
}

Stream.prototype.putch = function(c)
{
    if (this.buffer_size < 0)
        Errors.ioError(Constants.writeAtom, this.term);
    this.buffer.push(c);
    this.buffer_size++;
    return true;
}

Stream.prototype.putb = function(c)
{
    if (this.buffer_size < 0)
        Errors.ioError(Constants.writeAtom, this.term);
    this.buffer.push(c);
    this.buffer_size++;
    return true;
}

Stream.prototype.peekb = function()
{
    if (this.buffer_size == 0)
    {
        //debug_msg("Buffering...");
        this.buffer_size = this.read(this, 1, STREAM_BUFFER_SIZE, this.buffer);
        //debug_msg("Buffer now contains " + s.buffer_size + " elements");
    }
    if (this.buffer_size < 0)
        return this.buffer_size;
    // FIXME: Can this STILL be 0?
    if (this.buffer_size == 0)
        return -1;
    // At this point the buffer has some data in it
    return this.buffer[0];
}


module.exports = Stream;

/* Actual stream IO
var streams = [new_stream(null, stdout_write, null, null, null, "")];

function get_stream(term, ref)
{
    var fd = {};
    if (!get_stream_fd(term, fd))
        return false;
    ref.value = streams[fd.value];
    return true;
}

function get_stream_fd(term, s)
{
    if (TAG(term) != TAG_STR)
        return type_error("stream", term);
    ftor = VAL(memory[VAL(term)]);
    if (atable[ftable[ftor][0]] == "$stream" && ftable[ftor][1] == 1)
    {
        s.value = VAL(memory[VAL(term)+1]);
        return true;
    }
    return type_error("stream", term);
}

// Streams must all have a buffer to support peeking.
// If the buffer is empty, then fill it via read()


function _get_char(s)
{
    var t = s.getch(s);
    if (t == -1)
        return "end_of_file";
    else
        return String.fromCharCode(t);
}

function _peek_char(s)
{
    var t = peekch(s);
    if (t == -1)
        return "end_of_file";
    else
        return String.fromCharCode(t);
}

function _get_code(s)
{
    return getch(s);
}

function _peek_code(s)
{
    return peekch(s);
}


function get_stream_position(stream, property)
{
    if (stream.tell != null)
    {
        var p = stream.tell(stream) - stream.buffer.length;
        var ftor = lookup_functor('position', 1);
        var ref = alloc_structure(ftor);
        memory[state.H++] = p ^ (TAG_INT << WORD_BITS);
        return unify(ref, property);
    }
    return false;
}

var stream_properties = [get_stream_position];



function predicate_set_input(stream)
{
    var s = {};
    if (!get_stream_fd(stream, s))
        return false;
    current_input = s.value;
    return true;
}

function predicate_set_output(stream)
{
    var s = {};
    if (!get_stream_fd(stream, s))
        return false;
    current_output = s.value;
    return true;
}

function predicate_current_input(stream)
{   var ftor = lookup_functor('$stream', 1);
    var ref = alloc_structure(ftor);
    memory[state.H++] = current_input ^ (TAG_INT << WORD_BITS);
    return unify(stream, ref);
}

function predicate_current_output(stream)
{   var ftor = lookup_functor('$stream', 1);
    var ref = alloc_structure(ftor);
    memory[state.H++] = current_output ^ (TAG_INT << WORD_BITS);
    return unify(stream, ref);
}

function predicate_get_char(stream, c)
{
    var s = {};
    if (!get_stream(stream, s))
        return false;
    return unify(c, lookup_atom(_get_char(s.value)));
}

function predicate_get_code(stream, c)
{
    var s = {};
    if (!get_stream(stream, s))
        return false;
    return unify(c, (_get_code(s.value) & ((1 << (WORD_BITS-1))-1)) ^ (TAG_INT << WORD_BITS));
}

function predicate_get_byte(stream, c)
{
    var s = {};
    if (!get_stream(stream, s))
        return false;
    return unify(c, (getb(s.value) & ((1 << (WORD_BITS-1))-1)) ^ (TAG_INT << WORD_BITS));
}

function predicate_peek_char(stream, c)
{
    var s = {};
    if (!get_stream(stream, s))
        return false;
    return unify(c, lookup_atom(peek_char(s.value)));
}

function predicate_peek_code(stream, c)
{
    var s = {};
    if (!get_stream(stream, s))
        return false;
    return unify(c, _peek_code(s.value) ^ (TAG_INT << WORD_BITS));
}

function predicate_peek_byte(stream, c)
{
    var s = {};
    if (!get_stream(stream, s))
        return false;
    return unify(c, (peekb(s.value) & ((1 << (WORD_BITS-1))-1)) ^ (TAG_INT << WORD_BITS));
}

function predicate_put_char(stream, c)
{
    var s = {};
    if (!get_stream(stream, s))
        return false;
    return putch(s.value, atable[VAL(c)]);
}

function predicate_put_code(stream, c)
{
    var s = {};
    if (!get_stream(stream, s))
        return false;
    return putch(s.value, VAL(c));
}

function predicate_put_byte(stream, c)
{
    var s = {};
    if (!get_stream(stream, s))
        return false;
    return putb(s.value, VAL(c));
}

function predicate_flush_output(stream)
{
    var s = {};
    if (!get_stream(stream, s))
        return false;
    if (s.value.write != null)
    {
        return s.value.buffer_size == s.value.write(s.value, 1, s.value.buffer_size, s.value.buffer);
    }
    return permission_error("write", "stream", stream);
}

function predicate_at_end_of_stream(stream)
{
    var s = {};
    if (!get_stream(stream, s))
        return false;
    return (peekch(s.value) != -1);
}

function predicate_set_stream_position(s, position)
{
    var stream = {};
    if (!get_stream(s, stream))
        return false;
    stream = stream.value;
    if (stream.seek == null)
        return permission_error("seek", "stream", s);
    return stream.seek(stream, VAL(position));
}
function predicate_close(stream, options)
{
    var s = {};
    if (!get_stream(stream, s))
        return false;
    s = s.value;
    if (s.write != null)
    {
        // Flush output
        // FIXME: If flush fails, then what?
        s.write(s, 1, s.buffer_size, s.buffer);
    }
    if (s.close != null)
    {        
        // FIXME: Ignore s.close(s) if options contains force(true)
        return s.close(s);
    }
    // FIXME: Should be an error
    return false;
}


function predicate_stream_property(stream, property)
{
    var s = {};
    if (!get_stream(stream, s))
        return false;
    stream = s.value;
    var index = 0;
    if (state.foreign_retry)
    {
        index = state.foreign_value+1;
    }
    else
    {
        create_choicepoint();        
    }
    update_choicepoint_data(index);
    
    if (index >= stream_properties.length)
    {
        destroy_choicepoint();
        return false;
    }    
    return stream_properties[index](stream, property)
}

function predicate_current_stream(stream)
{
    var index = 0;
    if (state.foreign_retry)
    {
        index = state.foreign_value+1;
    }
    else
    {
        create_choicepoint();        
    }
    while (streams[index] === undefined)
    {
        if (index >= streams.length)
        {
            destroy_choicepoint();
            return false;
        }    
        index++;
    }
    update_choicepoint_data(index);
    var ftor = lookup_functor('$stream', 1);
    var ref = alloc_structure(ftor);
    memory[state.H++] = index ^ (TAG_INT << WORD_BITS);
    return unify(stream, ref);   
}
*/
