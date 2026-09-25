(
var project = thisProcess.nowExecutingPath.dirname.dirname.dirname;
var definition = SynthDef(\fmMatrixReference, {
    var freqs = [110, -220, 441, -997, 0, 12345];
    var phases = [0, 0.3, -1, 2, 0.7, 0.9];
    var amps = [0.5, -0.4, 0.7, 0.9, 0.6, 0.2];
    var actual = PMMatrix.ar(freqs, phases, amps);
    var expected = SinOsc.ar(freqs, phases, amps);
    Out.ar(0, actual ++ expected)
});
Routine({
    try {
        [44100, 48000].do { |sampleRate|
            [1, 64].do { |blockSize|
                var options = ServerOptions.new;
                var done = Semaphore.new(0), file, data, frames, maxError = 0, analyticError = 0;
                var tolerance;
                var path = project +/+ "build/reference.wav";
                var score = Score([
                    [0, [\d_recv, definition.asBytes]],
                    [0, [\s_new, \fmMatrixReference, 1000, 0, 0]],
                    [0.15, [\n_free, 1000]]
                ]);
                options.numInputBusChannels = 0;
                options.loadDefs = false;
                options.numOutputBusChannels = 12;
                options.blockSize = blockSize;
                options.memSize = 262144;
                options.ugenPluginsPath = [Platform.resourceDir +/+ "plugins", Platform.userExtensionDir +/+ "FMMatrixUGens"];
                score.recordNRT(outputFilePath: path, sampleRate: sampleRate,
                    headerFormat: "wav", sampleFormat: "float", options: options,
                    duration: 0.15, action: { done.signal });
                done.wait;
                file = SoundFile.openRead(path);
                if(file.isNil) { Error("Missing reference render").throw };
                frames = file.numFrames;
                data = FloatArray.newClear(frames * 12);
                file.readData(data); file.close;
                if(data.any { |x| x.isNaN or: { x.abs >= inf } }) {
                    Error("Non-finite reference render").throw
                };
                frames.do { |frame|
                    6.do { |i|
                        maxError = maxError.max((data[frame * 12 + i] - data[frame * 12 + 6 + i]).abs)
                    }
                    ;
                    if(data[frame * 12 + 4] != 0) {
                        6.do { |i|
                            var exact = [0.5, -0.4, 0.7, 0.9, 0.6, 0.2][i]
                                * sin([0, 0.3, -1, 2, 0.7, 0.9][i]
                                    + (2pi * [110, -220, 441, -997, 0, 12345][i] * frame / sampleRate));
                            analyticError = analyticError.max((data[frame * 12 + i] - exact).abs)
                        }
                    }
                };
                // SinOsc truncates its phase increment to an integer on an
                // 8192 * 65536 step circle. Bound drift by one step per frame,
                // plus 2e-7 for interpolated table/float error (SC 3.14.1).
                tolerance = (frames * 2pi / (8192 * 65536)) + 2e-7;
                "SinOsc reference: % Hz, block %, max error %, analytic error %"
                    .format(sampleRate, blockSize, maxError, analyticError).postln;
                if(data.abs.maxItem < 0.1 or: { maxError > tolerance } or: { analyticError > 2e-7 }) {
                    Error("Independent oscillator reference mismatch").throw
                }
            }
        };
        "FMMATRIX_NRT_OK".postln; 0.exit
    } { |error| error.reportError; "FMMATRIX_NRT_FAILED".postln; 1.exit }
}).play(SystemClock)
)
