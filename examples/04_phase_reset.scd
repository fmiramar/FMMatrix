(
s.waitForBoot {
    x = {
        var trigger = Impulse.ar(3);
        var ops = PMMatrix.ar([110, -221], 0, [1, 0.7], [[0.1, 1.3], [0.2, 0]],
            resetTrig: [trigger, 0], resetPhase: [pi/2, 0], oversample: 2);
        // Only operator 0 resets. Feedback memory continues across each reset.
        Splay.ar(ops) * 0.12 * Line.kr(0, 1, 0.05)
    }.play;
};
)
