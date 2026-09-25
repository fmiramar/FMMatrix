(
var useSupernova = ~fmMatrixUseSupernova ? false;
var server = Server(\fmMatrixVerification, NetAddr("127.0.0.1", if(useSupernova) { 57136 } { 57137 }));
var sizes = [1, 2, 3, 6, 8, 16, 31, 64];
var failed = false, peaks = Dictionary.new;
if(useSupernova) { Server.supernova } { Server.scsynth };
server.options.numOutputBusChannels = 2;
server.options.numInputBusChannels = 0;
server.options.memSize = 262144;
server.options.numWireBufs = 8192;
server.options.loadDefs = false;
server.options.ugenPluginsPath = [Platform.resourceDir +/+ "plugins", Platform.userExtensionDir +/+ "FMMatrixUGens"];
SystemClock.sched(50, { "FMMATRIX_SERVER_TIMEOUT".postln; server.quit; 1.exit; nil });
server.waitForBoot({
    Routine({
        try {
            var names = List.new, smallNames = List.new, waveBuffer;
            OSCdef(\matrixFail, { |message| message.postln; failed = true }, '/fail', server.addr);
            OSCdef(\matrixReply, { |message|
                var id = message[2], peak = message[3], bad = message[4];
                if(peak.isNaN or: { peak.abs >= inf } or: { bad != 0 }) { failed = true };
                peaks[id] = (peaks[id] ? 0).max(peak)
            }, '/matrixVerification', server.addr);
            waveBuffer = Buffer.alloc(server, 2048, 1);
            server.sync; waveBuffer.sine1([1, 0.2], asWavetable: false); server.sync;
            [PMMatrix, FMMatrix, PMMatrixZDF, PMGraph, FMMatrixExp, PMMatrixWave, PMMatrixIn, PMDelayGraph, PMMatrixNL].do { |klass|
                var isGraph = [PMGraph, PMDelayGraph].includes(klass);
                var testSizes = if(isGraph) { sizes ++ [128, 256] } { sizes };
                testSizes.do { |size|
                    var id = names.size;
                    var name = ("matrixVerify_" ++ klass.name ++ "_" ++ size).asSymbol;
                    var matrix = if(isGraph) { nil } { Array.fill(size, { Array.fill(size, 0.15 / size) }) };
                    SynthDef(name, {
                        var freqs = Array.series(size, 110, 7), factor = if(size <= 8) { 2 } { 1 };
                        var ops = case
                        { klass == PMGraph } { PMGraph.ar(freqs, 0.1, 0.7, (0..size-1), (1..size)%size, 0.15, oversample: factor) }
                        { klass == PMDelayGraph } { PMDelayGraph.ar(freqs, 0.1, 0.7, (0..size-1), (1..size)%size, 0.15, 0.002, 0.003, oversample: factor) }
                        { klass == PMMatrixWave } { PMMatrixWave.ar(freqs, 0.1, 0.7, matrix, waveBuffer, oversample: factor) }
                        { klass == PMMatrixIn } { PMMatrixIn.ar(freqs, 0.1, 0.7, matrix.collect { |row| row ++ [0.3] }, [SinOsc.ar(73)], oversample: factor) }
                        { klass == PMMatrixNL } { PMMatrixNL.ar(freqs, 0.1, 0.7, matrix, 1, oversample: factor) }
                        { klass.ar(freqs, 0.1, 0.7, matrix, oversample: factor) };
                        var bad = CheckBadValues.ar(ops.sum, post: 0);
                        SendReply.kr(Impulse.kr(40), '/matrixVerification',
                            [Amplitude.kr(ops[0]), A2K.kr(bad)], id);
                        Out.ar(0, Silent.ar(2))
                    }).send(server);
                    names.add(name);
                    if(size <= 6) { smallNames.add(name) }
                }
            };
            server.sync;
            names.do { |name|
                var node = Synth(name, target: server);
                0.09.wait; node.free
            };
            // 1000 real server create/free cycles, with audio blocks between
            // construction and destruction. Only this private test server is used.
            25.do {
                var nodes = Array.fill(40, { |i| Synth(smallNames.wrapAt(i), target: server) });
                server.sync; 0.02.wait;
                nodes.do(_.free); server.sync
            };
            server.sync;
            if(peaks.size != names.size or: { peaks.values.any { |peak| peak < 0.001 } } or: { failed }) {
                Error("Missing audio, nonfinite output, or server failure during lifecycle test").throw
            };
            OSCdef(\matrixFail).free; OSCdef(\matrixReply).free; waveBuffer.free;
            server.quit;
            0.3.wait;
            if(useSupernova) { "FMMATRIX_SUPERNOVA_OK".postln } { "FMMATRIX_SERVER_OK".postln };
            0.exit
        } { |error|
            error.reportError; server.quit; "FMMATRIX_SERVER_FAILED".postln; 1.exit
        }
    }).play(SystemClock)
}, 20, { "FMMATRIX_SERVER_BOOT_FAILED".postln; 1.exit });
)
