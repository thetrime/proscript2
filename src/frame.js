/* A frame looks like this:


/------------------------\
| arg0                   |
| arg1                   |
| ....                   |
| argN                   |
+------------------------+
| Local variables        |
| ...                    |
| ...                    |
+------------------------+
| pointer to code        |
+------------------------+
| pointer to parent frame|
+------------------------+
| parent PC              |
\------------------------/

*/

function Frame(parent)
{
    this.parent = parent;
    this.slots = [];
    this.code = undefined;
    this.returnPC = 0;
}


module.exports = Frame;
