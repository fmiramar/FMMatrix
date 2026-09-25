(
var project = thisProcess.nowExecutingPath.dirname.dirname.dirname;
var channels = 40;
var definition = SynthDef(\extensionReference, {
    var source = SinOsc.ar(79, 0.3), depth = SinOsc.ar(11, 0, 0.15, 0.2);
    var exp = FMMatrixExp.ar([-100, 0], [0.2, pi/2], [0.7, 1], [[0, 1], [0, 0]])[0];
    var external = PMMatrixIn.ar([123], 0.2, 0.7, [[0, depth]], [source])[0];
    var nl = PMMatrixNL.ar([123, 0], [0.2, pi/2], [0.7, 1], [[0, 2.5], [0, 0]], [1, 0])[0];
    var delayed = PMDelayGraph.ar([123], 0.2, 0.7, [0], [0], 0.3, 2.5/48000, 0.001)[0];
    var invalid = PMMatrixWave.ar([123], 0.2, bufnums: -1)[0];
    var local = LocalBuf.newFrom([0, 1, 0, -1]);
    var localDC = PMMatrixWave.ar([0], pi/2, bufnums: local)[0];
    var pairs = [1, 2, 4, 8].collect { |factor|
        var matrix = [[0.1, 0.2], [0.3, 0.1]], freqs = [123, -234];
        var wave = PMMatrixWave.ar(freqs, [0.1, 0.2], 0.7, matrix, [0, 0], oversample: factor);
        var dense = PMMatrix.ar(freqs, [0.1, 0.2], 0.7, matrix, oversample: factor);
        var inZero = PMMatrixIn.ar(freqs, [0.1, 0.2], 0.7, matrix, [], oversample: factor);
        var nlZero = PMMatrixNL.ar(freqs, [0.1, 0.2], 0.7, matrix, 0, oversample: factor);
        var delayMin = PMDelayGraph.ar(freqs, 0.1, 0.7, [0, 1], [1, 0], [depth, 0.3], 0, 0.001, oversample: factor);
        var graph = PMGraph.ar(freqs, 0.1, 0.7, [0, 1], [1, 0], [depth, 0.3], oversample: factor);
        [wave[0], dense[0], inZero[0], dense[0], nlZero[0], dense[0], delayMin[0], graph[0]]
    }.flatten;
    Out.ar(0, [DC.ar(1), exp, external, source, depth, nl, delayed, invalid+localDC] ++ pairs)
});
Routine({
    try {
        var done = Semaphore.new(0), options = ServerOptions.new, file, data;
        var first, last, errors = Array.fill(7, 0), delayedReference;
        var path = project +/+ "build/extension-reference.wav";
        var score = Score([
            [0, [\b_alloc, 0, 8192, 1, [\b_gen, 0, \sine1, 4, 1].asRawOSC]],
            [0, [\d_recv, definition.asBytes]],
            [0.02, [\s_new, \extensionReference, 1000, 0, 0]], [0.12, [\n_free, 1000]]]);
        options.numInputBusChannels = 0; options.numOutputBusChannels = channels; options.loadDefs = false;
        options.ugenPluginsPath = [Platform.resourceDir +/+ "plugins", Platform.userExtensionDir +/+ "FMMatrixUGens"];
        score.recordNRT(outputFilePath: path, sampleRate: 48000,
            headerFormat: "wav", sampleFormat: "float", options: options,
            duration: 0.12, action: { done.signal });
        done.wait; file = SoundFile.openRead(path);
        if(file.isNil) { Error("Missing extension render").throw };
        data = FloatArray.newClear(file.numFrames * channels); file.readData(data); file.close;
        if(data.any { |x| x.isNaN or: { x.abs >= inf } }) { Error("Nonfinite extension output").throw };
        first = (0 .. (data.size div: channels)-1).detect { |i| data[i*channels] > 0.5 };
        if(first.isNil) { Error("No extension audio").throw };
        last = first;
        while { data[last*channels] > 0.5 and: { last < ((data.size div: channels)-1) } } { last = last+1 };
        delayedReference = Array.fill(last-first, 0);
        (last-first).do { |frame|
            var offset = (frame+first)*channels, carrierPhase = (2pi*123*frame/48000) + 0.2.asFloat;
            var expPhase = if(frame == 0) { 0.2.asFloat } { 0.2.asFloat - (2pi*100*(1 + (2*(frame-1)))/48000) };
            var expectedExternal = 0.7.asFloat * sin(carrierPhase + (data[offset+3]*data[offset+4]));
            var expectedNL = 0.7.asFloat * sin(carrierPhase + if(frame == 0) { 0 } { 2.5.tanh });
            var actualDelay = (2.5/48000).asFloat * 48000, whole = actualDelay.floor.asInteger, fraction = actualDelay-whole;
            var older = if(frame >= whole) { delayedReference[frame-whole] } { 0 };
            var oldest = if(frame > whole) { delayedReference[frame-whole-1] } { 0 };
            delayedReference[frame] = 0.7.asFloat * sin(carrierPhase + (0.3.asFloat * ((older*(1-fraction)) + (oldest*fraction))));
            errors[0] = errors[0].max((data[offset+1] - (0.7.asFloat*sin(expPhase))).abs);
            errors[1] = errors[1].max((data[offset+2] - expectedExternal).abs);
            errors[2] = errors[2].max((data[offset+5] - expectedNL).abs);
            errors[3] = errors[3].max((data[offset+6] - delayedReference[frame]).abs);
            errors[4] = errors[4].max((data[offset+7]-1).abs);
            16.do { |pair|
                var error = (data[offset+8+(pair*2)] - data[offset+9+(pair*2)]).abs;
                var slot = if((pair%4) == 0) { 5 } { 6 };
                errors[slot] = errors[slot].max(error)
            }
        };
        "Extended server errors [exp, external, NL, delay, local/invalid, wave, equivalent]: %".format(errors).postln;
        if(errors.maxItem > 1e-6) { Error("Extended server reference mismatch").throw };
        "FMMATRIX_EXTENSIONS_OK".postln; 0.exit
    } { |error| error.reportError; "FMMATRIX_EXTENSIONS_FAILED".postln; 1.exit }
}).play(SystemClock)
)
