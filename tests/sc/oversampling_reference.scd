(
var project = thisProcess.nowExecutingPath.dirname.dirname.dirname;
var definition = SynthDef(\matrixOversamplingReference, {
    var outputs = [1, 2, 4, 8].collect { |factor|
        [PMMatrix.ar([1100], 0.2, 0.7, oversample: factor)[0],
         FMMatrix.ar([-1100], 0.2, 0.7, oversample: factor)[0]]
    }.flatten;
    Out.ar(0, outputs)
});
Routine({
    try {
        var done = Semaphore.new(0), options = ServerOptions.new, file, data, frames, maxError = 0;
        var path = project +/+ "build/oversampling-reference.wav";
        var score = Score([[0, [\d_recv, definition.asBytes]],
            [0, [\s_new, \matrixOversamplingReference, 1000, 0, 0]], [0.1, [\n_free, 1000]]]);
        options.numInputBusChannels = 0; options.numOutputBusChannels = 8; options.loadDefs = false;
        options.ugenPluginsPath = [Platform.resourceDir +/+ "plugins", Platform.userExtensionDir +/+ "FMMatrixUGens"];
        score.recordNRT(outputFilePath: path, sampleRate: 48000,
            headerFormat: "wav", sampleFormat: "float", options: options,
            duration: 0.1, action: { done.signal });
        done.wait; file = SoundFile.openRead(path);
        if(file.isNil) { Error("Missing oversampling render").throw };
        frames = file.numFrames; data = FloatArray.newClear(frames * 8);
        file.readData(data); file.close;
        if(data.any { |x| x.isNaN or: { x.abs >= inf } }) { Error("Nonfinite oversampling output").throw };
        8.do { |channel|
            var factor = [1, 2, 4, 8][channel div: 2];
            var delay = 31 * (1 - (1 / factor));
            var freq = if(channel.even) { 1100 } { -1100 };
            var error = 0;
            (256..4600).do { |frame|
                var expected = 0.7 * sin(0.2 + (2pi * freq * (frame - delay) / 48000));
                error = error.max((data[frame * 8 + channel] - expected).abs)
            };
            "Oversampling channel %, %x, latency %, error %".format(channel, factor, delay, error).postln;
            maxError = maxError.max(error)
        };
        if(maxError > 5e-6) { Error("Oversampling gain/latency mismatch").throw };
        "FMMATRIX_OVERSAMPLING_OK".postln; 0.exit
    } { |error| error.reportError; "FMMATRIX_OVERSAMPLING_FAILED".postln; 1.exit }
}).play(SystemClock)
)
