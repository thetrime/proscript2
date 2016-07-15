function Choicepoint(env, retryPC)
{
    this.frame = env.currentFrame;
    this.retryPC = retryPC;
    this.retryTR = env.TR;
    this.argP = env.argP;
    this.argI = env.argI;
    this.argS = env.argS;
    this.currentModule = env.currentModule;
    this.moduleStack = env.moduleStack;
    this.nextFrame = env.nextFrame;
    this.functor = env.currentFrame.functor;
}

Choicepoint.prototype.apply = function(env)
{
    env.currentFrame = this.frame;
    env.PC = this.retryPC;
    env.TR = this.retryTR;
    env.argP = this.argP;
    env.argI = this.argI;
    env.argS = this.argS;
    env.nextFrame = this.nextFrame;
    return (this.retryPC != -1)
}

module.exports = Choicepoint;
