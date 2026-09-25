# Equations and implementation decisions

Let `R` be the oversampling factor, `h = fs*R`, and `k` the internal sample
index. The output `y_i` includes the operator amplitude `a_i`, but excludes
language-side `mul/add`. All phases are double precision and wrap modulo 2π.
Ordinary edges use `old_j = y_j[k-R]`: exactly one host sample at every factor.
Rows are destinations and columns are sources. Histories initially contain zero.

| Class | Generation and phase update |
|---|---|
| PMMatrix | `m_i = sum(M_ij * old_j)`; `y_i = a_i*sin(phi_i+m_i)`; `phi_i += 2π*f_i/h` |
| FMMatrix | `y_i = a_i*sin(phi_i)`; `phi_i += 2π*(f_i+sum(M_ij*old_j))/h` |
| FMMatrixExp | `y_i = a_i*sin(phi_i)`; `phi_i += 2π*f_i*2^clamp(sum(M_ij*old_j),-128,128)/h` |
| PMMatrixZDF | Approximate `y_i = a_i*sin(phi_i+sum(M_ij*y_j))`; then advance carrier phase |
| PMGraph | PMMatrix equation evaluated only over the listed directed edges |
| PMMatrixWave | PMMatrix equation with cyclic interpolated raw table lookup replacing sine |
| PMMatrixIn | PMMatrix equation plus `sum(X_ik*x_k)` using current interpolated external input |
| PMDelayGraph | PMGraph equation with `old_j` replaced by a fractional history read at each edge's delay |
| PMMatrixNL | PMMatrix equation with `g_i(sum(M_ij*old_j))` replacing the untransformed sum |

ZDF starts from the preceding internal output and performs 1–64 simultaneous
Jacobi iterations. Each iteration blends the old guess with the new candidate
using damping in `(0,1]`. There is no traversal-dependent in-place update.
Convergence is not guaranteed; see solver residual data and the contraction-bound
tests. No alternative accelerated solver is exposed without measured benefit.

PMMatrixNL uses one transform per destination: identity, tanh, `x/(1+abs(x))`,
fold into `[-1,1]`, wrap into `[-1,1)`, abs, square, or sign with sign(0)=0.
The fold and wrap periods are 4 and 2. The transform is applied before carrier
phase addition. Matrix units scale the source into radians before transformation.

## Rates, reset and filtering

Control-rate depths ramp from the previous block target to the new target.
Optional one-pole lag applies to all matrix/edge depths, including audio rate,
with a 63.2% response time in seconds. It does not smooth delay times or frequency.
At higher internal rates, frequencies, amplitudes, depths, external sources and
delay times interpolate causally from preceding to current host values. Solver
damping is held during a host sample; table numbers are scalar/control rate and
are never interpolated. Topology, nonlinear shape and iteration budget are fixed.

A reset triggers on a nonpositive-to-positive host-sample edge. It sets carrier
phase before that sample's first internal step while retaining feedback/delay
history and solver guesses. Constructor output preview is followed by a complete
state reset so it does not consume the first audio sample.

The output decimator cascades 63-tap symmetric half-band filters. Its latencies
are 15.5, 23.25 and 27.125 host samples at 2x, 4x and 8x. Internal feedback uses
unfiltered operator output; filter latency is not added to the feedback loop.

## Memory and numerical limits

All UGen-owned storage is allocated in the constructor with the SC real-time
allocator and freed in the destructor. Calculation callbacks allocate nothing.
Sparse topology storage and work scale with N+E. Delayed graphs share one ring
per source, while each edge chooses its own fractional offset. Time clamps to
one host sample through maxDelay, with linear interpolation of history samples.

Non-finite signal inputs become zero; malformed structural inputs give silence.
Invalid tables give silence without falling back to another buffer. Output beyond
float range is bounded to finite float limits. Exponential FM bounds the summed
octave exponent to ±128; this is a numerical bound far beyond musical use.
Only smoothing state below `1e-300` is explicitly flushed. The plugin respects
the host thread's floating-point mode; it does not change that mode in callbacks.

Double summation was retained after cancellation experiments. The standard-library
sine was retained after accuracy/speed comparisons with table and polynomial
kernels. The four-term FIR window replaced a measured poorer three-term candidate.
Both ramping and optional lag remain available because they solve distinct
control changes. See [the benchmark report](../benchmarks/README.md).

## Scope completion

Milestones A–I are implemented and locally verified. PMDelayGraph is the plan's
sparse delayed-edge alternative to PMDelayMatrix. Optional DX algorithm/preset
helpers and accelerated solver methods are not part of this implementation.
Public tags, hosted CI results and external publication are separate release
actions; local verification is not a claim of cross-platform runtime coverage.
