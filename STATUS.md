# Status

## Completed

Implementation milestones A–I are complete: arbitrary-size core PM/linear FM,
dynamic controls/reset/smoothing, filtered oversampling, implicit ZDF, standalone
build/CI/packaging, sparse graphs, and the five extended synthesis variants.

The nine classes are PMMatrix, FMMatrix, PMMatrixZDF, PMGraph, FMMatrixExp,
PMMatrixWave, PMMatrixIn, PMDelayGraph and PMMatrixNL. PMDelayGraph implements
the plan's sparse delayed-edge alternative; a duplicate dense delay class is
not required. All have equations, argument documentation and runnable examples.

## Tests passed

- Strict Release builds of both macOS x64 server modules; seven native suites.
- ASan/UBSan for all seven suites, including the actual adapter, malformed
  layouts, invalid buffers, allocation failures and callback allocation guards.
- Language/API checks, scalar/control/audio inputs, output counts and errors.
- NRT independent oscillators at 44.1/48 kHz and blocks 1/64; mathematical error
  below 5.4e-8. PM/FM comparisons account for SinOsc's phase quantization.
- NRT reset/ramp/lag, oversampling latency/gain, ZDF recurrence and sparse/dense
  equivalence. Extended sine-table error is at most 1.2e-7; other extended
  equation checks are below 4.3e-8; equivalent graph/linear-shape cases match exactly.
- scsynth and Supernova: all nine classes, dense N=1/2/3/6/8/16/31/64,
  sparse N through 256, and 1000 Synth creation/free cycles on each server.
- SCDoc indexing and rendering of all nine class pages and the project guide.
- 23 runnable example checks: ten example files, three extra graph topologies,
  nine class examples and the project guide, on a muted private scsynth server.
- Optional installed-FM7 comparison: independent operators, one-way modulation,
  self-feedback and two hand-built stacks. Differences are recorded, not hidden.
- Windows x64 cross-compilation of both modules; binary formats/architectures
  inspected. Workflow YAML parses. This is not Windows runtime verification.

## Tests failed

No unresolved failure in completed local checks. The example harness exposed
the default interconnect-buffer limit at N=64; the first graph example now
uses N=16 and explains the server setting for larger arrays. The corrected
examples pass. GitHub jobs have not run.

## Known limitations

- Linux, macOS arm64 and Windows native runtime validation awaits hosted CI.
- Public repository/tag/release publication has not been performed.
- ZDF can remain nonconvergent for strong matrices; oversampling does not make
  arbitrary feedback or discontinuous waveforms alias-free.
- Wavetables require raw mono cycle samples. Table switches can click; delay
  modulation can change pitch, and delay histories need time to fill.
- Optional DX convenience/preset layers and accelerated solver modes are outside
  the implemented scope.

## Performance

The benchmark report includes alias spectra, dense/sparse CPU, solver residuals,
504 actual-adapter input-rate/budget cases, summation/sine decisions, denormal
behavior and FM7 comparison data. On the recorded macOS x64 run, the N=64 dense
adapter exceeded real time even at 1x. Sparse ring processing was approximately
3.2x/5.6x/10.4x faster than the native dense engine at N=64/128/256. These are
measured examples, not fixed thresholds or operator caps.

## API changes

Operator count is fixed by `freqs`. Dense matrices use destination rows and
source columns; PMMatrixIn adds external columns, while graph classes use fixed
endpoint lists. Outputs are always Arrays. Core controls are initial phases,
amplitudes, resetTrig/resetPhase, oversample, smooth and output mul/add.
Class help gives exact signatures and units; no published API has been renamed.

## Next milestone

The FM/PM implementation is ready for local use. Release administration remains:
review/commit the source and benchmark data, run the configured hosted matrix,
and publish only after platform results and release approval. SCShader is the
next implementation project requested by the user.
