(
s.waitForBoot {
    x = {
        var morph = SinOsc.kr(0.15).range(0, 1);
        var matrix = [[morph * 0.3, morph * 2, 0], [0, 0, 1-morph], [morph * 0.5, 0, 0]];
        var ops = PMMatrix.ar([110, 220, 331], 0, [1, 0.7, 0.5], matrix, smooth: 0.02);
        Splay.ar(ops) * 0.12 * Line.kr(0, 1, 0.05)
    }.play;
};
)
