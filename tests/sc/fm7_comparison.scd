(
var project = thisProcess.nowExecutingPath.dirname.dirname.dirname;
var fm7 = \FM7.asClass;
Routine {
    try {
        var csv, cases;
        if(fm7.isNil) { Error("Optional comparison requires the installed sc3-plugins FM7 class and binary.").throw };
        cases = [
            ["independent", []],
            ["one_way", [[0, 1, 0.3]]],
            ["self_feedback", [[0, 0, 0.3]]],
            ["two_stacks", [[0, 1, 0.3], [1, 2, 0.3], [3, 4, 0.3], [4, 5, 0.3]]]
        ];
        csv = File(project +/+ "benchmarks/fm7_comparison.csv", "w");
        csv.write("patch,comparison,rms_difference,max_difference,fm7_rms,comparison_rms\n");
        cases.do { |entry|
            var matrix = Array.fill(6, { Array.fill(6, 0) });
            var done = Semaphore.new(0), options = ServerOptions.new, definition, score, file, data;
            var sums = Array.fill(3, 0), peaks = Array.fill(3, 0), powers = Array.fill(4, 0), count = 0;
            var alignedSum = 0, alignedPeak = 0, alignedCount = 0, alignedPowers = [0, 0];
            var path = project +/+ "build/fm7-comparison.wav";
            entry[1].do { |edge| matrix[edge[0]][edge[1]] = edge[2] };
            definition = SynthDef(\fm7Comparison, {
                var freqs = [110, 173, 227, 293, 347, 401], phases = Array.fill(6, 0.2), amps = Array.fill(6, 0.7);
                var original = fm7.ar(Array.fill(6, { |i| [freqs[i], phases[i], amps[i]] }), matrix);
                var pm = PMMatrix.ar(freqs, phases, amps, matrix);
                var over = PMMatrix.ar(freqs, phases, amps, matrix, oversample: 2);
                var implicit = PMMatrixZDF.ar(freqs, phases, amps, matrix, iterations: 8);
                Out.ar(0, [DC.ar(1)] ++ original ++ pm ++ over ++ implicit)
            });
            options.numInputBusChannels = 0; options.numOutputBusChannels = 25; options.loadDefs = false;
            options.ugenPluginsPath = [Platform.resourceDir +/+ "plugins", Platform.userExtensionDir +/+ "FMMatrixUGens", fm7.filenameSymbol.asString.dirname];
            score = Score([[0, [\d_recv, definition.asBytes]], [0, [\s_new, \fm7Comparison, 1000, 0, 0]], [0.2, [\n_free, 1000]]]);
            score.recordNRT(outputFilePath: path, sampleRate: 48000,
                headerFormat: "wav", sampleFormat: "float", options: options,
                duration: 0.2, action: { done.signal });
            done.wait; file = SoundFile.openRead(path);
            if(file.isNil) { Error("Missing FM7 comparison render").throw };
            data = FloatArray.newClear(file.numFrames*25); file.readData(data); file.close;
            if(data.any { |value| value.isNaN or: { value.abs >= inf } }) { Error("Nonfinite FM7 comparison output").throw };
            (data.size div: 25).do { |frame|
                var offset = frame*25;
                if(data[offset] > 0.5) {
                    6.do { |i|
                        var original = data[offset+1+i];
                        powers[0] = powers[0] + original.squared;
                        3.do { |variant|
                            var value = data[offset+7+(variant*6)+i], difference = value-original;
                            sums[variant] = sums[variant] + difference.squared;
                            peaks[variant] = peaks[variant].max(difference.abs);
                            powers[variant+1] = powers[variant+1] + value.squared
                        };
                        if((offset+25) < data.size and: { data[offset+25] > 0.5 }) {
                            var advanced = data[offset+32+i], difference = advanced-original;
                            alignedSum = alignedSum + difference.squared;
                            alignedPeak = alignedPeak.max(difference.abs);
                            alignedPowers[0] = alignedPowers[0] + original.squared;
                            alignedPowers[1] = alignedPowers[1] + advanced.squared;
                            alignedCount = alignedCount+1
                        };
                        count = count+1
                    }
                }
            };
            if(count < 100 or: { powers.any { |power| power < 0.1 } }) { Error("Silent or incomplete FM7 comparison").throw };
            ["PMMatrix_1x", "PMMatrix_2x_unaligned", "PMMatrixZDF_8_iterations"].do { |name, i|
                csv.write("%,%,%,%,%,%\n".format(entry[0], name, sqrt(sums[i]/count), peaks[i], sqrt(powers[0]/count), sqrt(powers[i+1]/count)))
            };
            csv.write("%,PMMatrix_1x_advanced_one_sample,%,%,%,%\n".format(entry[0], sqrt(alignedSum/alignedCount), alignedPeak,
                sqrt(alignedPowers[0]/alignedCount), sqrt(alignedPowers[1]/alignedCount)));
            "FM7_COMPARISON % RMS differences: %".format(entry[0], sums.collect { |sum| sqrt(sum/count) }).postln
        };
        csv.close;
        "FMMATRIX_FM7_OK".postln; 0.exit
    } { |error| error.reportError; "FMMATRIX_FM7_FAILED".postln; 1.exit }
}.play(SystemClock)
)
