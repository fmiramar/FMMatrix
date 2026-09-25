# DSP measurements

Run `build/fm_matrix_benchmark benchmarks` after a Release build to regenerate
the CSV files. The checked-in data format is intended for publishable numeric
results; generated audio and build products are not source files.

## Oversampling

`alias_PMMatrix.csv` and `alias_FMMatrix.csv` measure energy outside the legitimate
unaliased sideband bins of a single-modulator patch. The sample rate is 48 kHz,
FFT length 16384, carrier bin 3500, modulator bin 2303, and PM index 4 (FM depth
is four times the modulator frequency in Hz). A 4096-sample warmup precedes the
coherent, unwindowed FFT. Legitimate bins are `±(carrier + k * modulator)` below
Nyquist; energy in other bins represents folded sidebands and numerical error.
Values are relative to total output energy, not absolute dBFS. This measures a
specific patch and does not imply that arbitrary feedback networks are alias-free.

The measured PM alias fractions are approximately -5.4, -67.6, -117.1 and
-117.1 dB at 1x/2x/4x/8x. Linear FM gives approximately -5.1, -67.0, -117.1
and -117.1 dB. At least 10 dB improvement over 1x is required by the benchmark.

The decimator uses cascaded 63-tap symmetric half-band FIR filters with a
four-term Blackman-Harris window. Native frequency-response tests find maximum
passband error and stopband amplitude about 3.11e-6 for normalized frequencies
0–0.18 and 0.32–0.5 respectively. A three-term Blackman candidate measured
1.10e-4 and was replaced before integration. The last stage attenuates the
upper audio band; the transition is approximately 0.36–0.64 times host rate.
Group delay is 0, 15.5, 23.25, or 27.125 host samples. Feedback uses unfiltered
internal output with a separate full-host-sample delay at every factor.

## CPU and memory

`oversampling.csv` records median elapsed time over three runs, using the shared
native DSP engine with constant inputs, in Release on macOS x86_64. The run
length adapts to network size (512–8192 host samples). `realtime_fraction_48k`
is elapsed compute time divided by simulated audio time; it is not the server's
CPU meter and excludes input conditioning, scheduling and other Synths.
`dsp_state_bytes` includes the engine and its delay/filter buffers, but excludes
the server wrapper's input caches. Do not use these numbers as hard CI limits.

On this run, 64-operator linear FM at 8x exceeded real time even before server
overhead. Large mostly-zero networks should use sparse `PMGraph`.
Benchmark on the target system and leave audio scheduling headroom.

## Implicit solver

`solver.csv` sweeps operator count, matrix infinity norm, iteration count and
damping. It measures the last 512 samples of a 1024-sample run at 48 kHz.
Residual is `y - a*sin(phase + M*y)` at the phase used to generate that sample.
Timing includes residual measurement and is not a pure solver CPU benchmark.
All rows remained finite, including nonconvergent configurations.

For the two-operator norm-0.2 network at full damping, RMS residual falls from
about 2.2e-6 at four iterations to 2.7e-9 at eight and 5.5e-15 at sixteen.
The norm-2.5 network remains near 0.9 RMS residual even at 64 iterations.
More iterations do not repair a noncontractive fixed-point map automatically.
The native root test also checks the theoretical contraction error bound:
negative self-coupling of -0.8 need not reach 1e-8 error on its first sample
even with 64 iterations, but settles when the next sample starts from that guess.

## Sparse networks

`sparse.csv` compares the same ring at 1x using a dense mostly-zero matrix and
an E=N edge list, over 4096 samples. Native sparse processing was approximately
3.2x faster at 64 operators, 5.6x at 128 and 10.4x at 256 in this run. The
benchmark excludes the server adapter; its sparse input caches also scale as
N+E. CPU readings vary with compiler and machine load, so ratios are evidence,
not fixed performance guarantees. Native and NRT tests compare dense/sparse
output separately; benchmarks do not replace correctness tests.

## Server-adapter input rates

Run `build/fm_matrix_adapter_benchmark benchmarks` to regenerate `input_rates.csv`.
It measures the actual server calculation callback with mock host allocation,
including input caches, control ramps, 5 ms matrix lag, output conversion and
DSP. It excludes server scheduling, hardware I/O and upstream modulation UGens.
The 504 cases cover N=1/2/4/8/16/32/64, all four factors, scalar/control/audio
matrix inputs, and ZDF budgets 1/2/4/8. Control targets alternate each block;
audio coefficients use a precomputed varying block. Reported times are the
median of three runs of 4–32 blocks after warmup, at block size 64.

Including adapter work, N=64 PM/FM already consumed roughly 1.2 times the
available 48 kHz sample period at 1x on this macOS x64 run. At 8x it consumed
roughly 3.7–3.9 times. These are practical warnings about dense scale, not API
limits. Even lower measured fractions need scheduling headroom; compare sparse
routing before increasing a mostly-zero dense matrix.

## Numerical decisions

Run `build/fm_matrix_numerics_benchmark benchmarks` for the following experiments:

- `summation.csv` uses float-representable values `[1e8, 0.25, -1e8, 0.25]`
  repeatedly in one row, with a long-double reference. At N=256 float summation
  returns 0.25 instead of 32; double matches this reference exactly. This is an
  adversarial cancellation case, not an estimate of typical musical error.
- `sine.csv` compares standard sine, an interpolated 8192-point float table, and
  a folded seventh-degree Taylor approximation over a coherent 137-cycle signal
  of 65536 samples. Maximum errors are approximately 1.1e-16, 1.01e-7 and
  1.57e-4. The table was slower than standard sine in this implementation;
  the polynomial was faster but less accurate. The production sine remains
  `std::sin`; the comparison table is an idiom study, not SC's exact SinOsc kernel.
  Spectral error is residual energy after fitting sine/cosine at the fundamental,
  relative to a unit sine's power, with a -300 dB numerical reporting floor.
- `denormals.csv` measures a unity self-feedback state with zero frequency and
  seeds 0, 1e-100 and 1e-310. On x86, raw IEEE subnormal processing took about
  43 ns/sample versus about 7.6 ns for a small normal value. With flush-to-zero
  enabled, the subnormal became zero and cost about 4.4 ns/sample. The benchmark
  restores the original thread mode. Production callbacks retain the host's mode;
  SC's x86 servers already enable flush-to-zero. The mode-switch experiment is
  omitted on non-x86 builds. Explicit smoothing-state flushing is documented
  separately in the project guide.

These microbenchmarks resolve implementation choices; they are not universal
speed guarantees. Filter candidates, solver damping and iteration counts, and
control smoothing are covered by the earlier response/residual data and NRT tests.

## FM7 migration

With sc3-plugins FM7 installed, run
`python3 tools/run_sc_tests.py --fm7 --only fm7_comparison`.
`fm7_comparison.csv` measures six independent operators, a single edge, self
feedback and two hand-built stacks, using 0.2-second 48 kHz renders. It compares
1x PM, unaligned 2x PM, and eight-iteration ZDF with FM7. An additional diagnostic
advances the 1x PM result by one sample. See [migration notes](../notes/FM7_MIGRATION.md).
