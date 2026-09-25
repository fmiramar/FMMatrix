(
var project = thisProcess.nowExecutingPath.dirname.dirname.dirname;
var definition = SynthDef(\matrixDynamicReference, {
    var freqs = [SinOsc.ar(17, 0, 30, 100), SinOsc.ar(11, 0, 20, -200)];
    var amps = [SinOsc.ar(19, 0, 0.1, 0.7), SinOsc.ar(23, 0, 0.1, 0.5)];
    var depths = [SinOsc.ar(71, 0, 0.3), SinOsc.ar(97, 0, 0.2)];
    var triggers = [DC.ar(1), Impulse.ar(313)];
    var resets = [DC.ar(0.7), SinOsc.ar(3, 0, 0.5)];
    var matrix = [[0, depths[0]], [depths[1], 0]];
    var pm = PMMatrix.ar(freqs, [0.1, -0.2], amps, matrix, triggers, resets);
    var fm = FMMatrix.ar(freqs, [0.1, -0.2], amps, matrix, triggers, resets);
    var depth = \depth.kr(0);
    var ramp = PMMatrix.ar([0, 0], [pi/2, 0], 1, [[0, 0], [depth, 0]])[1];
    var lag = PMMatrix.ar([0, 0], [pi/2, 0], 1, [[0, 0], [depth, 0]], smooth: 0.01)[1];
    // K2A itself ramps control inputs. A stationary pi/2 carrier exposes
    // the raw amplitude control target for the independent ramp reference.
    Out.ar(0, pm ++ fm ++ freqs ++ amps ++ depths ++ triggers ++ resets ++ [PMMatrix.ar([0], pi/2, depth)[0], ramp, lag])
});
Routine({
    try {
        [1, 64].do { |blockSize|
            var done = Semaphore.new(0), options = ServerOptions.new, file, data, frames;
            var maxError = 0, rampError = 0, lagError = 0, lastTarget = 0, start = 0, smooth = 0;
            var pmPhase = [0.1.asFloat, -0.2.asFloat], fmPhase = pmPhase.copy;
            var pmPrevious = [0, 0], fmPrevious = [0, 0], triggerPrevious = [0, 0];
            var path = project +/+ "build/dynamic-reference.wav";
            var score = Score([[0, [\d_recv, definition.asBytes]],
                [0, [\s_new, \matrixDynamicReference, 1000, 0, 0]],
                [0.04, [\n_set, 1000, \depth, 0.9]],
                [0.08, [\n_set, 1000, \depth, -0.4]], [0.12, [\n_free, 1000]]]);
            options.numInputBusChannels = 0; options.numOutputBusChannels = 17;
            options.blockSize = blockSize; options.loadDefs = false;
            options.ugenPluginsPath = [Platform.resourceDir +/+ "plugins", Platform.userExtensionDir +/+ "FMMatrixUGens"];
            score.recordNRT(outputFilePath: path, sampleRate: 48000,
                headerFormat: "wav", sampleFormat: "float", options: options,
                duration: 0.12, action: { done.signal });
            done.wait; file = SoundFile.openRead(path);
            if(file.isNil) { Error("Missing dynamic render").throw };
            frames = file.numFrames; data = FloatArray.newClear(frames * 17);
            file.readData(data); file.close;
            if(data.any { |x| x.isNaN or: { x.abs >= inf } }) { Error("Nonfinite dynamic render").throw };
            frames.do { |frame|
                var offset = frame * 17, pmNext = [0, 0], fmNext = [0, 0];
                if(data[offset + 12] != 0) {
                    2.do { |i|
                        var trig = data[offset + 10 + i], reset = data[offset + 12 + i];
                        if(trig > 0 and: { triggerPrevious[i] <= 0 }) { pmPhase[i] = reset; fmPhase[i] = reset };
                        triggerPrevious[i] = trig;
                        pmNext[i] = data[offset + 6 + i] * sin(pmPhase[i] + (data[offset + 8 + i] * pmPrevious[1-i]));
                        fmNext[i] = data[offset + 6 + i] * sin(fmPhase[i]);
                        pmPhase[i] = (pmPhase[i] + (2pi * data[offset + 4 + i] / 48000)).wrap(-pi, pi);
                        fmPhase[i] = (fmPhase[i] + (2pi * (data[offset + 4 + i] + (data[offset + 8 + i] * fmPrevious[1-i])) / 48000)).wrap(-pi, pi);
                        maxError = maxError.max((pmNext[i] - data[offset + i]).abs).max((fmNext[i] - data[offset + 2 + i]).abs)
                    };
                    pmPrevious = pmNext; fmPrevious = fmNext;
                    if(frame % blockSize == 0) { start = lastTarget; lastTarget = data[offset + 14] };
                    {
                        var ramp = start + ((lastTarget - start) * ((frame % blockSize + 1) / blockSize));
                        smooth = smooth + ((1 - exp(-1 / (0.01 * 48000))) * (ramp - smooth));
                        rampError = rampError.max((data[offset + 15] - sin(ramp)).abs);
                        lagError = lagError.max((data[offset + 16] - sin(smooth)).abs)
                    }.value
                }
            };
            "Dynamic block %: recurrence %, ramp %, lag %".format(blockSize, maxError, rampError, lagError).postln;
            if(maxError > 3e-7 or: { rampError > 1e-7 } or: { lagError > 1e-7 }) { Error("Dynamic recurrence mismatch").throw }
        };
        "FMMATRIX_DYNAMIC_OK".postln; 0.exit
    } { |error| error.reportError; "FMMATRIX_DYNAMIC_FAILED".postln; 1.exit }
}).play(SystemClock)
)
