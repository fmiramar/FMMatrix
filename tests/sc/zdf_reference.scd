(
var project = thisProcess.nowExecutingPath.dirname.dirname.dirname;
var definition = SynthDef(\zdfReference, {
    var trig = Impulse.ar(123);
    var outputs = PMMatrixZDF.ar([110, -201], [0.1, -0.2], [0.8, 0.7],
        [[0.2, 0.1], [-0.3, 0.2]], iterations: 8, damping: 0.7,
        resetTrig: [trig, 0], resetPhase: 0.4);
    Out.ar(0, outputs ++ [trig])
});
Routine({
    try {
        var done = Semaphore.new(0), options = ServerOptions.new, file, data, maxError = 0;
        var phase = [0.1, -0.2], previous = [0, 0], lastTrigger = 0;
        var path = project +/+ "build/zdf-reference.wav";
        var score = Score([[0, [\d_recv, definition.asBytes]],
            [0, [\s_new, \zdfReference, 1000, 0, 0]], [0.1, [\n_free, 1000]]]);
        options.numInputBusChannels = 0; options.numOutputBusChannels = 3; options.loadDefs = false;
        options.ugenPluginsPath = [Platform.resourceDir +/+ "plugins", Platform.userExtensionDir +/+ "FMMatrixUGens"];
        score.recordNRT(outputFilePath: path, sampleRate: 48000,
            headerFormat: "wav", sampleFormat: "float", options: options,
            duration: 0.1, action: { done.signal });
        done.wait; file = SoundFile.openRead(path);
        if(file.isNil) { Error("Missing ZDF render").throw };
        data = FloatArray.newClear(file.numFrames * 3); file.readData(data); file.close;
        4600.do { |frame|
            var trigger = data[frame * 3 + 2];
            if(trigger > 0 and: { lastTrigger <= 0 }) { phase[0] = 0.4 };
            lastTrigger = trigger;
            8.do {
                var next = [0, 0];
                2.do { |i|
                    var sum = [[0.2, 0.1], [-0.3, 0.2]][i] * previous;
                    next[i] = (0.3 * previous[i]) + (0.7 * [0.8, 0.7][i] * sin(phase[i] + sum.sum))
                };
                previous = next
            };
            2.do { |i|
                maxError = maxError.max((previous[i] - data[frame * 3 + i]).abs);
                phase[i] = (phase[i] + (2pi * [110, -201][i] / 48000)).wrap(-pi, pi)
            }
        };
        "Damped ZDF recurrence max error: %".format(maxError).postln;
        if(maxError > 2e-7 or: { data.abs.maxItem < 0.5 }) { Error("ZDF recurrence mismatch").throw };
        "FMMATRIX_ZDF_OK".postln; 0.exit
    } { |error| error.reportError; "FMMATRIX_ZDF_FAILED".postln; 1.exit }
}).play(SystemClock)
)
