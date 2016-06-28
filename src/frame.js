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

function Frame(env)
{
    this.parent = env.currentFrame;
    this.slots = [];
    this.code = undefined;
    this.returnPC = 0;
    this.choicepoint = env.choicepoints.length;
}


module.exports = Frame;
