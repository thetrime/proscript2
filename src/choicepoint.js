function Choicepoint(env, retryPC)
{
    this.frame = env.currentFrame;
    this.retryPC = retryPC;
    this.retryTR = env.TR;
    this.argP = env.argP;
    this.argI = env.argI;
    this.argS = env.argS;
    this.code = env.currentFrame.code;
    this.nextFrame = env.nextFrame;
    this.functor = env.currentFrame.functor;
}


module.exports = Choicepoint;
