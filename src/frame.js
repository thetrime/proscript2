"use strict";
exports=module.exports;

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
|pointer to curent clause|
+------------------------+
| pointer to parent frame|
+------------------------+
| parent PC              |
\------------------------/

*/

function Frame(env)
{
    this.parent = env.currentFrame;
    this.depth = 0;
    if (env.currentFrame != undefined)
        this.depth = env.currentFrame.depth + 1;
    this.slots = [];
    this.reserved_slots = [];
    this.clause = undefined;
    this.contextModule = env.currentModule;
    this.returnPC = 0;
    //console.log("Created a new frame " + this.depth + ", with choicepoint is set to " + env.choicepoints.length);
    this.choicepoint = env.choicepoints.length;
}


module.exports = Frame;
