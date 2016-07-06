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
    this.do_buffer = true;
    this.term = new BlobTerm("stream", this);
}

Stream.prototype.flush = function()
{
    if (this.write == null)
        Errors.permissionError(Constants.outputAtom, Constants.streamAtom, this.term);
    return this.buffer_size == this.write(this, this.buffer_size, this.buffer);
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
        this.buffer_size = this.read(this, STREAM_BUFFER_SIZE, this.buffer);
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
    if (this.do_buffer == false)
        this.flush();
    return true;
}

Stream.prototype.putb = function(c)
{
    if (this.buffer_size < 0)
        Errors.ioError(Constants.writeAtom, this.term);
    this.buffer.push(c);
    this.buffer_size++;
    if (this.do_buffer == false)
        this.flush();
    return true;
}

Stream.prototype.peekb = function()
{
    if (this.buffer_size == 0)
    {
        //debug_msg("Buffering...");
        this.buffer_size = this.read(this, STREAM_BUFFER_SIZE, this.buffer);
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

Stream.string_read = function(stream, count, buffer)
{
    var bytes_read = 0;
    var records_read;
    for (records_read = 0; records_read < count; records_read++)
    {
        t = stream.data.shift();
        if (t === undefined)
        {
            return records_read;
        }
        buffer[bytes_read++] = t;
    }
    return records_read;
}

module.exports = Stream;
