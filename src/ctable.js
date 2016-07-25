"use strict";
exports=module.exports;

// Top 3 bits indicate the tag:
// 11: constant (in this table)
// 10: compound
// 01:
// 00: variable

function CTable()
{
    this.entries = {};
    this.table = [];
    this.next = 0;
}

CTable.prototype.intern = function(object, hash)
{
    hash = hash || object.hashCode();
    if (this.entries[hash] === undefined)
    {
        var index = this.next;
        this.next++;
        this.table[index] = object;
        this.entries[hash] = [{value: object,
                               index: index}];
        return index;
    }
    else
    {
        var bucket = this.entries[hash];
        for (var i = 0; i < bucket.length; i++)
        {
            if (bucket[i].value.equals(object))
                return bucket[i].index;
        }
        var index = this.next;
        this.next++;
        this.table[index] = object;
        bucket.push({value: object,
                     index: index});
        return index;
    }
}

CTable.prototype.get = function(index)
{

    return this.table[index & 0x3fffffff];
}



var ctable = new CTable();


module.exports = ctable;
