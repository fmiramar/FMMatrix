# Changelog

## 0.1.0 (unreleased)

- Milestone I: octave-valued `FMMatrixExp`, raw cyclic-buffer `PMMatrixWave`,
  rectangular `PMMatrixIn`, fractional-delay `PMDelayGraph`, and per-destination
  `PMMatrixNL`, each with explicit equations, native/NRT tests and rendered help.
- All nine classes verified on scsynth and Supernova, including lifecycle stress,
  invalid-buffer/structural-input checks and real-time allocation failures.
- Ten standalone examples and all help examples exercised on a muted server;
  large-graph setup now documents interconnect-buffer requirements.
- Added actual-adapter input-rate/solver-budget benchmarks, numerical precision,
  sine-kernel and denormal experiments, plus optional FM7 migration renders.
- Full standalone packaging and reproducible verification instructions; public
  release publication and hosted-platform validation are pending.

- Milestone H: `PMGraph` with O(N+E) storage/work, fixed endpoints, dynamic
  depths, shared reset/smoothing/oversampling, dense equivalence tests through
  256 operators, graph examples and a dense/sparse benchmark.

- Milestone G: adapter ASan/UBSan and allocation-failure checks, 1000-cycle
  scsynth/Supernova tests, bounded language-test runner, explicit-platform CI
  and account/project/version/platform/architecture packaging with SHA-256.

- Milestone F: `PMMatrixZDF` with bounded simultaneous fixed-point iteration,
  damping, reset, oversampling, bisection/reference tests and solver CSVs.

- Milestone E: cascaded FIR 2x/4x/8x oversampling with preserved full-host-sample
  network delay, causal input interpolation, latency tests and alias/CPU data.

- Milestone D: sample-accurate per-operator reset, audio-rate controls, control
  matrix ramps, optional coefficient lag and a playable matrix-morph example.
- Single-operator calls now return a one-element output Array consistently.

- Milestone C: `FMMatrix`, true linear FM in Hz, negative instantaneous frequency,
  shared allocation and oscillator engine, recurrence and server reference tests.

- Milestone B: sine operators with double phase accumulation, signed frequencies,
  synchronous dense PM, reference/permutation tests and NRT SinOsc comparisons.

- Establish Milestone A: standalone `PMMatrix` arbitrary-operator
  `MultiOutUGen` skeleton, real-time allocation/destruction, structural layout
  validation, native layout tests, language compilation test, and server smoke
  test.
