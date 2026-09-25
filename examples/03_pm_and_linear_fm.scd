(
// PM on the left (radians), linear FM on the right (Hz).
// Equal matrix numbers would mean different modulation strengths.
s.waitForBoot {
    x = {
        var depth = SinOsc.kr(0.13).range(0.1, 3);
        var pm = PMMatrix.ar([220, 110], 0, [1, 1], [[0, depth], [0, 0]], oversample: 2)[0];
        var fm = FMMatrix.ar([220, 110], 0, [1, 1], [[0, depth*110], [0, 0]], oversample: 2)[0];
        [pm, fm] * 0.12 * Line.kr(0, 1, 0.05)
    }.play;
};
)
