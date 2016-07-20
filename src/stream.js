"use strict";
exports=module.exports;

var BlobTerm = require('./blob_term');
var STREAM_BUFFER_SIZE = 2048;

// read(stream, ptr, size, buffer)
//    Request to read (up to) size bytes to buffer[ptr]
// write(stream, ptr, size, buffer)
//    Request to write (up to) size bytes from buffer[ptr]


function Stream(read, write, seek, close, tell, userdata)
{
    this.read = read;
    this.write = write;
    this.seek = seek;
    this.close = close;
    this.tell = tell;
    this.data = userdata;
    this.buffer = Buffer(STREAM_BUFFER_SIZE);
    this.filled_buffer_size = 0;
    this.buffer_ptr = 0;
    this.do_buffer = true;
    this.term = new BlobTerm("stream", this);
}

Stream.prototype.flush = function()
{
    if (this.write == null)
        Errors.permissionError(Constants.outputAtom, Constants.streamAtom, this.term);
    var bytesAvailableToFlush = this.filled_buffer_size - this.buffer_ptr;
    if (bytesAvailableToFlush > 0)
    {
        var flushed =  this.write(this, this.buffer_ptr, bytesAvailableToFlush, this.buffer);
        if (flushed < 0)
            return false;
        if (flushed == bytesAvailableToFlush)
        {
            this.buffer_ptr = 0;
            this.filled_buffer_size = 0;
            return true;
        }
        else
            abort("Failed to flush buffer");
    }

    return true; // Nothing to write
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
    var i = this.buffer.buffer_ptr;
    for (var mask = 0x20; mask != 0; mask >>=1 )
    {
        if (i > this.buffer.filled_buffer_size)
        {
            // This is a problem. We need to buffer more data! But we must also not lose the existing buffer since we are peeking.
            abort("Unicode break in peekch. This is a bug");
        }
        var next = this.buffer.readUInt8(i+1);
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
    if (this.buffer_ptr == this.filled_buffer_size)
    {
        //console.log("Buffering...");
        this.filled_buffer_size = 0;
        this.buffer_ptr = 0;
        this.filled_buffer_size = this.read(this, 0, STREAM_BUFFER_SIZE, this.buffer);
        //console.log("Buffer now contains " + this.filled_buffer_size + " elements");
    }
    if (this.filled_buffer_size < 0) // Error condition
        return this.filled_buffer_size;
    // FIXME: Can this STILL be 0?
    if (this.filled_buffer_size == 0)
        return -1;
    // At this point the buffer has some data in it. Return the next byte
    return this.buffer.readUInt8(this.buffer_ptr++);
}

Stream.prototype.putch = function(c)
{
    if (this.filled_buffer_size < 0) // Error condition
        Errors.ioError(Constants.writeAtom, this.term);
    this.buffer.writeUInt8(c, this.filled_buffer_size++);
    if (this.filled_buffer_size == STREAM_BUFFER_SIZE || this.do_buffer == false)
        return this.flush();
    return true;
}

Stream.prototype.putb = function(c)
{
    if (this.filled_buffer_size < 0) // Error condition
        Errors.ioError(Constants.writeAtom, this.term);
    this.buffer.writeUInt8(c, this.filled_buffer_size++);
    if (this.filled_buffer_size == STREAM_BUFFER_SIZE || this.do_buffer == false)
        return this.flush();
    return true;
}

Stream.prototype.peekb = function()
{
    if (this.buffer_ptr == this.filled_buffer_size)
    {
        //debug_msg("Buffering...");
        this.filled_buffer_size = 0;
        this.buffer_ptr = 0;
        this.filled_buffer_size = this.read(this, 0, STREAM_BUFFER_SIZE, this.buffer);
        //debug_msg("Buffer now contains " + s.buffer_size + " elements");
    }
    if (this.filled_buffer_size < 0) // Error condition
        return this.filled_buffer_size;
    // FIXME: Can this STILL be 0?
    if (this.filled_buffer_size == 0)
        return -1;
    // At this point the buffer has some data in it. Return the next byte
    return this.buffer.readUInt8(this.buffer_ptr);
}

Stream.string_read = function(stream, offset, length, buffer)
{
    var bytesToRead = length;
    if (stream.data.buffer.length < stream.data.ptr + length)
        bytesToRead = stream.data.buffer.length - stream.data.ptr;
    stream.data.buffer.copy(buffer, offset, stream.data.ptr, stream.data.ptr+bytesToRead);
    stream.data.ptr += bytesToRead;
    return bytesToRead;
}

Stream.stringBuffer = function(string)
{
    return {buffer: new Buffer(string, "utf-8"),
            ptr: 0};
}

module.exports = Stream;
