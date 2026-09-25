(
s.waitForBoot {
    Routine {
        b = Buffer.readChannel(s, ExampleFiles.child, channels: [0]);
        s.sync;
        SynthDef(\matrixExternalMorph, { |out=0, buf|
            var source = PlayBuf.ar(1, buf, BufRateScale.kr(buf), loop: 1);
            // var source = SoundIn.ar(0); // alternative live input
            var morph = SinOsc.kr(0.08).range(0.2, 1);
            var matrix = [[0.1, 0.4, 3*morph], [0.2, 0, 1.5*morph]];
            var ops = PMMatrixIn.ar([110, 221], 0, [1, 0.7], matrix, [source],
                oversample: 2, smooth: 0.02);
            Out.ar(out, Splay.ar(ops) * 0.12 * Line.kr(0, 1, 0.05));
        }).add;
        s.sync;
        x = Synth(\matrixExternalMorph, [\buf, b]);
    }.play;
};
// After stopping x, free the sample buffer with b.free.
)
