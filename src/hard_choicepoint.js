var Constants = require('./constants');

function HardChoicepoint(env)
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
    if (env.currentFrame != null)
        this.functor = env.currentFrame.functor;
    else
        this.functor = Constants.undefinedPredicateFunctor;
}

HardChoicepoint.prototype.canApply = function(discardForeign)
{
    return discardForeign;
}


HardChoicepoint.prototype.apply = function(env)
{
    env.currentFrame = this.frame;
    env.PC = 0;
    env.TR = this.retryTR;
    env.argP = this.argP;
    env.argI = this.argI;
    env.argS = this.argS;
    env.nextFrame = this.nextFrame;
    return true;
}

module.exports = HardChoicepoint;
