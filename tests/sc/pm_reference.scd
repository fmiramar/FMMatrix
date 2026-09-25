(
var project = thisProcess.nowExecutingPath.dirname.dirname.dirname;
var definition = SynthDef(\pmMatrixEdgeReference, {
    var actual = PMMatrix.ar([173, 431], 0, [0.7, 0.8], [[0, 0], [2, 0]]);
    var source = SinOsc.ar(173, 0, 0.7);
    var expected = SinOsc.ar(431, Delay1.ar(source) * 2, 0.8);
    var silent = PMMatrix.ar([100, 200], mul: 0, add: [0.1, -0.2]);
    var fm = FMMatrix.ar([173, -431], 0, [0.7, 0.8], [[0, 0], [750, 0]]);
    var fmReference = SinOsc.ar(-431 + (750 * Delay1.ar(source)), 0, 0.8);
    Out.ar(0, [actual[1], expected] ++ K2A.ar(silent) ++ [fm[1], fmReference])
});
Routine({
    try {
        var done = Semaphore.new(0), options = ServerOptions.new, file, data, frames, maxError = 0, fmError = 0;
        var path = project +/+ "build/pm-reference.wav";
        var score = Score([[0, [\d_recv, definition.asBytes]],
            [0, [\s_new, \pmMatrixEdgeReference, 1000, 0, 0]], [0.2, [\n_free, 1000]]]);
        options.numInputBusChannels = 0; options.numOutputBusChannels = 6;
        options.loadDefs = false;
        options.ugenPluginsPath = [Platform.resourceDir +/+ "plugins", Platform.userExtensionDir +/+ "FMMatrixUGens"];
        score.recordNRT(outputFilePath: path, sampleRate: 48000,
            headerFormat: "wav", sampleFormat: "float", options: options,
            duration: 0.2, action: { done.signal });
        done.wait;
        file = SoundFile.openRead(path);
        if(file.isNil) { Error("Missing PM render").throw };
        frames = file.numFrames; data = FloatArray.newClear(frames * 6);
        file.readData(data); file.close;
        if(data.any { |x| x.isNaN or: { x.abs >= inf } }) { Error("Nonfinite PM output").throw };
        frames.do { |frame|
            maxError = maxError.max((data[frame * 6] - data[frame * 6 + 1]).abs);
            fmError = fmError.max((data[frame * 6 + 4] - data[frame * 6 + 5]).abs);
            if(data[frame * 6 + 2] != 0) {
                if((data[frame * 6 + 2] - 0.1).abs > 1e-7 or: { (data[frame * 6 + 3] + 0.2).abs > 1e-7 }) {
                    Error("Multichannel mul/add mismatch").throw
                }
            }
        };
        "One-sample PM edge reference max error: %".format(maxError).postln;
        "Linear FM edge reference max error: %".format(fmError).postln;
        // SinOsc phase quantization propagates through a depth-2 PM edge.
        if(maxError > 0.0003 or: { fmError > 0.0006 } or: { data.abs.maxItem < 0.5 }) { Error("PM/FM reference mismatch").throw };
        "PMMATRIX_NRT_OK".postln; 0.exit
    } { |error| error.reportError; "PMMATRIX_NRT_FAILED".postln; 1.exit }
}).play(SystemClock)
)
