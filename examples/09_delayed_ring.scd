(
s.waitForBoot {
    x = {
        var times = SinOsc.kr([0.07, 0.09, 0.11, 0.13]).range(0.002, 0.008);
        var ops = PMDelayGraph.ar([110, 173, 227, 293], 0, 0.7,
            [0, 1, 2, 3], [1, 2, 3, 0], 1.3, times, maxDelay: 0.01, oversample: 2);
        // Changing delay times bends the modulation history in time.
        Splay.ar(ops) * 0.1 * Line.kr(0, 1, 0.05)
    }.play;
};
)
