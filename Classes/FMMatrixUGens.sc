// SPDX-License-Identifier: GPL-3.0-or-later
FMMatrixBase : MultiOutUGen {
    *operatorVectors { |freqs, phases=0, amps=1|
        var frequencies = freqs.asArray;
        var operators, phaseValues, ampValues;
        if(frequencies.isEmpty) {
            Error("%: freqs must contain at least one operator.".format(this.name)).throw
        };
        operators = frequencies.size;
        phaseValues = this.expandOperatorValues(phases, operators, "phases");
        ampValues = this.expandOperatorValues(amps, operators, "amps");
        ^this.validateSignals([operators] ++ frequencies ++ phaseValues ++ ampValues)
    }

    *operatorInputs { |freqs, phases=0, amps=1, modMatrix|
        var inputs = this.operatorVectors(freqs, phases, amps);
        var operators = inputs[0], rows, flattened;
        if(((operators * operators) + (5 * operators) + 3) > 2147483647) {
            Error("%: matrix layout exceeds the SynthDef input-count limit.".format(this.name)).throw
        };
        rows = modMatrix ?? {
            Array.fill(operators, { Array.fill(operators, 0) })
        };
        rows = rows.asArray;
        if(rows.size != operators) {
            Error("PMMatrix: modMatrix must be % x % because freqs contains % operators."
                .format(operators, operators, operators)).throw
        };
        rows.do { |row|
            if(row.asArray.size != operators) {
                Error("PMMatrix: modMatrix must be % x % because freqs contains % operators."
                    .format(operators, operators, operators)).throw
            }
        };
        flattened = rows.collect { |row| row.asArray }.flatten(1);
        ^inputs ++ this.validateSignals(flattened)
    }

    *validateSignals { |values|
        if(values.any { |value| value.isNumber.not and: { value.isKindOf(UGen).not } }) {
            Error("%: every input must be a number or UGen; nested arrays and non-signals are invalid.".format(this.name)).throw
        };
        ^values
    }

    *expandOperatorValues { |values, operators, label|
        var result = values.asArray;
        if(result.size == 1) {
            result = Array.fill(operators, { result[0] })
        };
        if(result.size != operators) {
            Error("PMMatrix: % must be a scalar or contain % values."
                .format(label, operators)).throw
        };
        ^this.validateSignals(result)
    }
}

PMMatrix : FMMatrixBase {
    *matrixInputs { |freqs, phases, amps, modMatrix, resetTrig, resetPhase, oversample, smooth|
        var inputs = this.operatorInputs(freqs, phases, amps, modMatrix);
        ^this.appendControls(inputs, resetTrig, resetPhase, oversample, smooth)
    }

    *appendControls { |inputs, resetTrig, resetPhase, oversample, smooth|
        this.validateSignals([oversample, smooth]);
        if([1, 2, 4, 8].includes(oversample).not) { Error("%: oversample must be the constant 1, 2, 4 or 8.".format(this.name)).throw };
        if(smooth.rate == \audio) { Error("%: smooth must be scalar or control-rate.".format(this.name)).throw };
        inputs = inputs ++ this.expandOperatorValues(resetTrig, inputs[0], "resetTrig")
            ++ this.expandOperatorValues(resetPhase, inputs[0], "resetPhase") ++ [oversample, smooth];
        ^inputs
    }

    *ar { |freqs, phases=0, amps=1, modMatrix, resetTrig=0, resetPhase=0, oversample=1, smooth=0, mul=1, add=0|
        var inputs = this.matrixInputs(freqs, phases, amps, modMatrix, resetTrig, resetPhase, oversample, smooth);
        // MultiOutUGen consumes the first N as its SynthDef output count.
        // The duplicate N remains server input 0 for defensive layout checks.
        ^this.multiNewList([\audio, inputs[0]] ++ inputs).asArray.madd(mul, add)
    }

    *kr {
        Error("% is audio-rate only; use %.ar.".format(this.name, this.name)).throw
    }

    init { |numOutputs ... theInputs|
        inputs = theInputs;
        ^this.initOutputs(numOutputs, rate)
    }

    argNamesInputsOffset { ^2 }
}

PMGraph : PMMatrix {
    *graphInputs { |freqs, phases, amps, sources, destinations, depths, resetTrig, resetPhase, oversample, smooth|
        var inputs = this.operatorVectors(freqs, phases, amps);
        var n = inputs[0], edges;
        sources = sources.asArray; destinations = destinations.asArray;
        edges = sources.size;
        if(destinations.size != edges) { Error("PMGraph: sources and destinations must have equal length.").throw };
        if((4 + (5*n) + (3*edges)) > 2147483647) { Error("PMGraph: layout exceeds SynthDef input-count limit.").throw };
        (sources ++ destinations).do { |index|
            if(index.isNumber.not or: { index.isNaN } or: { index < 0 } or: { index >= n } or: { index != index.asInteger }) {
                Error("PMGraph: edge indices must be constant integers in 0..%.".format(n-1)).throw
            }
        };
        inputs = inputs ++ [edges] ++ sources ++ destinations ++ this.expandOperatorValues(depths, edges, "depths");
        ^this.appendControls(inputs, resetTrig, resetPhase, oversample, smooth)
    }

    *ar { |freqs, phases=0, amps=1, sources=#[ ], destinations=#[ ], depths=0, resetTrig=0, resetPhase=0, oversample=1, smooth=0, mul=1, add=0|
        var inputs = this.graphInputs(freqs, phases, amps, sources, destinations, depths, resetTrig, resetPhase, oversample, smooth);
        ^this.multiNewList([\audio, inputs[0]] ++ inputs).asArray.madd(mul, add)
    }
}

FMMatrix : PMMatrix { }
FMMatrixExp : PMMatrix { }

PMMatrixWave : PMMatrix {
    *ar { |freqs, phases=0, amps=1, modMatrix, bufnums=0, resetTrig=0, resetPhase=0, oversample=1, smooth=0, mul=1, add=0|
        var inputs = this.matrixInputs(freqs, phases, amps, modMatrix, resetTrig, resetPhase, oversample, smooth);
        var tables = this.expandOperatorValues(bufnums.asArray.collect(_.asUGenInput), inputs[0], "bufnums");
        if(tables.any { |item| item.rate == \audio }) { Error("PMMatrixWave: bufnums must be scalar or control-rate.").throw };
        ^this.multiNewList([\audio, inputs[0]] ++ inputs ++ tables).asArray.madd(mul, add)
    }
}

PMMatrixIn : PMMatrix {
    *ar { |freqs, phases=0, amps=1, modMatrix, external=#[ ], resetTrig=0, resetPhase=0, oversample=1, smooth=0, mul=1, add=0|
        var inputs = this.operatorVectors(freqs, phases, amps);
        var n = inputs[0], k, columns, rows;
        external = this.validateSignals(external.asArray); k = external.size; columns = n+k;
        if((4 + (5*n) + k + (n*columns)) > 2147483647) { Error("PMMatrixIn: layout exceeds SynthDef input-count limit.").throw };
        rows = (modMatrix ?? { Array.fill(n, { Array.fill(columns, 0) }) }).asArray;
        if(rows.size != n or: { rows.any { |row| row.asArray.size != columns } }) {
            Error("PMMatrixIn: modMatrix must have % rows and % columns (N internal, then K external sources).".format(n, columns)).throw
        };
        inputs = inputs ++ [k] ++ external ++ this.validateSignals(rows.collect(_.asArray).flatten(1));
        inputs = this.appendControls(inputs, resetTrig, resetPhase, oversample, smooth);
        ^this.multiNewList([\audio, n] ++ inputs).asArray.madd(mul, add)
    }
}

PMDelayGraph : PMGraph {
    *ar { |freqs, phases=0, amps=1, sources=#[ ], destinations=#[ ], depths=0, delays=0.002, maxDelay=0.05, resetTrig=0, resetPhase=0, oversample=1, smooth=0, mul=1, add=0|
        var inputs = this.graphInputs(freqs, phases, amps, sources, destinations, depths, resetTrig, resetPhase, oversample, smooth);
        var times = this.expandOperatorValues(delays, sources.asArray.size, "delays");
        if(maxDelay.isNumber.not or: { maxDelay.isNaN } or: { maxDelay <= 0 } or: { maxDelay >= inf }) {
            Error("PMDelayGraph: maxDelay must be a finite positive construction-time constant in seconds.").throw
        };
        if((inputs.size + times.size + 1) > 2147483647) { Error("PMDelayGraph: layout exceeds SynthDef input-count limit.").throw };
        ^this.multiNewList([\audio, inputs[0]] ++ inputs ++ times ++ [maxDelay]).asArray.madd(mul, add)
    }
}

PMMatrixNL : PMMatrix {
    *ar { |freqs, phases=0, amps=1, modMatrix, shapes=0, resetTrig=0, resetPhase=0, oversample=1, smooth=0, mul=1, add=0|
        var inputs = this.matrixInputs(freqs, phases, amps, modMatrix, resetTrig, resetPhase, oversample, smooth);
        shapes = this.expandOperatorValues(shapes, inputs[0], "shapes");
        shapes.do { |shape|
            if(shape.isNumber.not or: { shape.isNaN } or: { shape < 0 } or: { shape > 7 } or: { shape != shape.asInteger }) {
                Error("PMMatrixNL: shapes must be constant integers 0..7.").throw
            }
        };
        ^this.multiNewList([\audio, inputs[0]] ++ inputs ++ shapes).asArray.madd(mul, add)
    }
}

PMMatrixZDF : PMMatrix {
    *ar { |freqs, phases=0, amps=1, modMatrix, iterations=4, damping=1, resetTrig=0, resetPhase=0, oversample=1, smooth=0, mul=1, add=0|
        var inputs = this.matrixInputs(freqs, phases, amps, modMatrix, resetTrig, resetPhase, oversample, smooth);
        if(iterations.isNumber.not or: { iterations.isNaN } or: { iterations < 1 } or: { iterations > 64 } or: { iterations != iterations.asInteger }) {
            Error("PMMatrixZDF: iterations must be a constant integer in 1..64.").throw
        };
        this.validateSignals([damping]);
        if(damping.isNumber and: { damping.isNaN or: { damping <= 0 } or: { damping > 1 } }) {
            Error("PMMatrixZDF: damping must be greater than 0 and at most 1.").throw
        };
        ^this.multiNewList([\audio, inputs[0]] ++ inputs ++ [iterations, damping]).asArray.madd(mul, add)
    }
}
