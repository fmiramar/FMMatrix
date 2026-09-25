(
// Slow crossfade from 1x (left endpoint) to 4x (right endpoint).
// The outputs are not latency-aligned: the 4x filter adds 23.25 samples.
s.waitForBoot {
    x = {
        var matrix = [[0, 3], [0, 0]];
        var a = PMMatrix.ar([3200, 1700], 0, 1, matrix, oversample: 1)[0];
        var b = PMMatrix.ar([3200, 1700], 0, 1, matrix, oversample: 4)[0];
        var mix = XFade2.ar(a, b, SinOsc.kr(0.15));
        (mix * 0.08 * Line.kr(0, 1, 0.05)) ! 2
    }.play;
};
)
