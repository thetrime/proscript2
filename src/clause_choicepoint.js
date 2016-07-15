function ClauseChoicepoint(env)
{
    this.frame = env.currentFrame;
    this.retryPC = 0;
    this.retryTR = env.TR;
    this.argP = env.argP;
    this.argI = env.argI;
    this.argS = env.argS;
    this.currentModule = env.currentModule;
    this.moduleStack = env.moduleStack;
    this.nextFrame = env.nextFrame;
    this.functor = env.currentFrame.functor;
}


ClauseChoicepoint.prototype.apply = function(env)
{
    env.currentFrame = this.frame;
    env.PC = 0;
    env.TR = this.retryTR;
    env.argP = this.argP;
    env.argI = this.argI;
    env.argS = this.argS;
    env.nextFrame = this.nextFrame;

    if (env.currentFrame.clause.nextClause == null)
        return false;
    env.currentFrame.clause = env.currentFrame.clause.nextClause;
    return true;
}

module.exports = ClauseChoicepoint;
