(
// Two different raw cycles. Avoid Osc's interleaved wavetable format here.
s.waitForBoot {
    Routine {
        b = [Buffer.alloc(s, 2048, 1), Buffer.alloc(s, 4096, 1)];
        s.sync;
        b[0].sine1([1, 0.3, 0.1], asWavetable: false);
        b[1].sine1([1, 0, 0.2, 0, 0.05], asWavetable: false);
        s.sync;
        x = {
            var ops = PMMatrixWave.ar([110, 173], 0, [1, 0.7],
                [[0.15, 1], [0.3, 0]], b, oversample: 2);
            Splay.ar(ops) * 0.1 * Line.kr(0, 1, 0.05)
        }.play;
    }.play;
};
// After stopping x, free both buffers with b.do(_.free).
)
