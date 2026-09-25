(
var project = thisProcess.nowExecutingPath.dirname.dirname.dirname;
var definition = SynthDef(\graphReference, {
    var freqs = [110, -201, 317];
    var depths = [SinOsc.ar(31, 0, 0.2, 0.2), 0.3, 0.4, 0.1];
    var sources = [0, 1, 2, 0], destinations = [1, 2, 0, 0];
    var matrix = [[depths[3], 0, depths[2]], [depths[0], 0, 0], [0, depths[1], 0]];
    var pairs = [1, 2, 4, 8].collect { |factor|
        var graph = PMGraph.ar(freqs, 0.2, 0.7, sources, destinations, depths, oversample: factor);
        var dense = PMMatrix.ar(freqs, 0.2, 0.7, matrix, oversample: factor);
        [graph[0], dense[0]]
    }.flatten;
    Out.ar(0, pairs)
});
Routine({
    try {
        var done = Semaphore.new(0), options = ServerOptions.new, file, data, maxError = 0;
        var path = project +/+ "build/graph-reference.wav";
        var score = Score([[0, [\d_recv, definition.asBytes]],
            [0, [\s_new, \graphReference, 1000, 0, 0]], [0.1, [\n_free, 1000]]]);
        [32, 64, 128, 256].do { |size|
            var count;
            var bytes = SynthDef(\graphCompile, {
                var ops = PMGraph.ar(Array.series(size, 100, 1), sources: (0..size-1), destinations: (1..size) % size, depths: 0.2);
                count = ops.size;
                SendReply.kr(Impulse.kr(1), '/graphCount', A2K.kr(ops[0]));
                Out.ar(0, Silent.ar(2))
            }).asBytes;
            if(count != size or: { bytes.size < 1 }) { Error("Graph output-count mismatch").throw }
        };
        options.numInputBusChannels = 0; options.numOutputBusChannels = 8; options.loadDefs = false;
        options.ugenPluginsPath = [Platform.resourceDir +/+ "plugins", Platform.userExtensionDir +/+ "FMMatrixUGens"];
        score.recordNRT(outputFilePath: path, sampleRate: 48000,
            headerFormat: "wav", sampleFormat: "float", options: options,
            duration: 0.1, action: { done.signal });
        done.wait; file = SoundFile.openRead(path);
        if(file.isNil) { Error("Missing graph render").throw };
        data = FloatArray.newClear(file.numFrames * 8); file.readData(data); file.close;
        if(data.any { |x| x.isNaN or: { x.abs >= inf } }) { Error("Nonfinite graph output").throw };
        (data.size div: 8).do { |frame|
            4.do { |pair| maxError = maxError.max((data[(frame*8) + (pair*2)] - data[(frame*8) + (pair*2) + 1]).abs) }
        };
        "Dense/sparse server error: %".format(maxError).postln;
        if(maxError > 1e-6 or: { data.abs.maxItem < 0.1 }) { Error("Sparse/dense reference mismatch").throw };
        "FMMATRIX_GRAPH_OK".postln; 0.exit
    } { |error| error.reportError; "FMMATRIX_GRAPH_FAILED".postln; 1.exit }
}).play(SystemClock)
)
