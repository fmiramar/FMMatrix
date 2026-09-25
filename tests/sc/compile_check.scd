(
var sizes = [1, 2, 3, 6, 8, 16, 31, 64];
var valid = true;
[PMMatrix, FMMatrix, FMMatrixExp, PMMatrixWave, PMMatrixIn, PMMatrixNL, PMMatrixZDF].do { |klass| sizes.do { |size|
    var matrix = Array.fill(size, { Array.fill(size, 0) });
    var outputCount;
    var definition = SynthDef("fmMatrixLayoutCompile_" ++ size, {
        var outputs = klass.ar(Array.series(size, 100, 11), 0, 1, matrix);
        outputCount = outputs.asArray.size;
        SendReply.kr(Impulse.kr(1), '/matrixCompile', outputs.asArray.collect(A2K.kr(_)));
        Out.ar(0, Silent.ar(2))
    });
    var bytes = definition.asBytes;
    valid = valid and: { outputCount == size } and: { bytes.size > 0 }
        and: { definition.children.count { |ugen| ugen.class == klass } == 1 };
} };
[
    { PMMatrix.ar([]) },
    { PMMatrix.ar([100, 200], phases: [0, 1, 2]) },
    { FMMatrix.ar([100, 200], amps: []) },
    { PMMatrix.ar([100, 200], modMatrix: [[0], [0]]) },
    { PMMatrix.ar([[100, 200]]) },
    { PMMatrix.ar([100], resetTrig: [0, 1]) },
    { PMMatrix.ar([100], oversample: 3) },
    { PMMatrix.ar([100], smooth: [0, 1]) },
    { PMMatrixZDF.ar([100], iterations: 0) },
    { PMMatrixZDF.ar([100], iterations: 65) },
    { PMMatrixZDF.ar([100], damping: 0) },
    { PMMatrixZDF.ar([100], damping: 2) },
    { PMGraph.ar([100], sources: [0], destinations: []) },
    { PMGraph.ar([100], sources: [-1], destinations: [0]) },
    { PMGraph.ar([100], sources: [0], destinations: [1]) },
    { PMGraph.ar([100], sources: [0.5], destinations: [0]) },
    { PMGraph.ar([100], sources: [0, 0], destinations: [0, 0], depths: [1, 2, 3]) },
    { PMMatrixWave.ar([100, 200], bufnums: [0, 1, 2]) },
    { PMMatrixIn.ar([100], modMatrix: [[0]], external: [1]) },
    { PMMatrixNL.ar([100], shapes: 8) },
    { PMMatrixNL.ar([100], shapes: 1.5) },
    { PMDelayGraph.ar([100], maxDelay: 0) },
    { PMDelayGraph.ar([100], maxDelay: inf) },
    { PMDelayGraph.ar([100], sources: [0], destinations: [0], delays: [0.1, 0.2]) }
].do { |badCall|
    var rejected = false;
    try { badCall.value } { rejected = true };
    valid = valid and: { rejected }
};
[PMGraph, PMDelayGraph].do { |klass|
    [1, 32, 64, 128, 256].do { |size|
        var outputs;
        var definition = SynthDef(\extendedGraphCompile, {
            outputs = klass.ar(Array.series(size, 100, 1), sources: (0..size-1), destinations: (1..size)%size, depths: 0.2);
            Out.ar(0, outputs[0])
        });
        valid = valid and: { outputs.size == size } and: { definition.asBytes.size > 0 }
    }
};
if(valid) { "FMMATRIX_COMPILE_OK".postln; 0.exit } {
    "FMMATRIX_COMPILE_FAILED".postln; 1.exit
};
)
