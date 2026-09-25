# FMMatrixUGens

![FMMatrix Animation](gif-generation/PMGraph_Waveform_V4.gif)

FMMatrixUGens is a standalone SuperCollider server-plugin family by fmiramar,
developed with Codex, for phase- and frequency-modulation networks of arbitrary
construction-time size. It independently extends the matrix idea documented by
sc3-plugins [FM7](https://doc.sccode.org/Classes/FM7.html).

This project is classified as an experimental adaptation: it changes operator
count, feedback equations, input-rate support and oscillator types. FM7 is not
required, and this is not a DX7 emulator. Version 0.1.0 implements all nine
planned synthesis classes. Local validation is complete; hosted platform CI
and public release publication remain separate steps. See [STATUS.md](STATUS.md).

## UGens

- `PMMatrix` generates dense sine-operator PM networks with depths in radians.
  Every destination reads the complete output vector from one host sample earlier.
- `FMMatrix` generates true linear FM with depths in Hz.
  Modulation changes phase accumulation and supports negative instantaneous frequency.
- `PMMatrixZDF` approximates same-sample implicit PM.
  Bounded, damped simultaneous fixed-point iterations expose an explicit accuracy/CPU tradeoff.
- `PMGraph` generates sparse PM networks with fixed endpoints and dynamic depths.
  It evaluates an edge list in O(N+E) work per internal sample.
- `FMMatrixExp` generates exponential FM with octave-valued depths.
  Its frequency law preserves the sign of the base frequency and bounds extreme exponents numerically.
- `PMMatrixWave` uses user-supplied raw mono cycle buffers for each operator.
  Cyclic linear interpolation replaces sine lookup, and invalid buffers produce silence.
- `PMMatrixIn` lets external audio modulate internal oscillators.
  An Nx(N+K) matrix separates delayed internal sources from current external sources.
- `PMDelayGraph` gives each sparse connection an independent fractional delay.
  Shared per-source history rings reduce storage, with a minimum delay of one host sample.
- `PMMatrixNL` applies a nonlinear function to each destination's modulation sum.
  Eight explicit shapes include tanh, fold, wrap and square, evaluated once per destination.

All classes expose raw operator outputs, double-precision phase, signed base
frequencies, phase reset, dynamic modulation depths, optional coefficient lag,
and filtered 1x/2x/4x/8x processing. See the installed `FMMatrixUGens` help guide,
[equations and design notes](docs/IMPLEMENTATION.md), and [examples](examples).

## Matrix convention

The dense matrix uses row-major destination/source orientation:
`M[i][j]` means source operator `j` modulates destination operator `i`.
PM depths are radians per unit source amplitude; linear FM uses Hz and exponential
FM uses octaves. Dense matrices are square except for `PMMatrixIn`'s extra
external-source columns. Sparse classes use zero-based source/destination lists.
`freqs` fixes N; the result is always an Array, including N=1. `mul/add` apply
after the network and do not change internal modulation.

## Build and install

For a binary bundle, copy its `Extensions/FMMatrixUGens` folder into your
SuperCollider user Extensions directory (`Platform.userExtensionDir` in sclang).
Choose the package matching your operating system and CPU architecture.

To build, use CMake, a C++17 compiler, and SuperCollider source matching the
installed server. A Supernova build also needs the SDK's `nova-tt` submodule:

```sh
cmake -S . -B build -DSC_PATH=/path/to/supercollider \
  -DCMAKE_BUILD_TYPE=Release -DSCSYNTH=ON -DSUPERNOVA=OFF
cmake --build build --config Release
ctest --test-dir build --output-on-failure
cmake --install build --prefix "/path/to/SuperCollider/Extensions"
```

Set `-DSUPERNOVA=ON` to build both server modules. On macOS pass
`-DCMAKE_OSX_ARCHITECTURES=x86_64` or `arm64` explicitly. With Visual Studio,
configure Windows x64 with `-A x64` and add `-C Release` to the ctest invocation.

Recompile the class library and restart the audio server after installation.
From a source checkout, run `python3 tools/run_sc_tests.py --live --supernova`
after installing both modules. Omit `--supernova` for a scsynth-only build.
See [release and CI instructions](docs/RELEASE.md) for verification and packaging.

## Limits and safety

The operator count is fixed when a SynthDef is built. It is not globally capped
at a small DX-style number; practical limits follow the dense `N²` SynthDef
input layout, server interconnect buffers, available real-time memory, and CPU.
Malformed layouts are rejected by the sclang wrapper and produce silence in the
server UGen if a hand-authored SynthDef bypasses that wrapper.

Oversampling adds 0, 15.5, 23.25 or 27.125 host samples of filter latency and
increases CPU cost. It reduces aliasing in measured patches; strong modulation
or discontinuous tables can still alias. ZDF can fail to converge for strong
matrices even at its maximum iteration count. Delay histories start empty;
use a short fade-in and short initial delay settings.

Large dense networks can exceed real time even at 1x; sparse graphs are usually
more practical when most connections are zero. Large output arrays may require
a higher `s.options.numWireBufs` before server boot, and long delay graphs may
need more `s.options.memSize`. See [measurements](benchmarks/README.md).

## Source and provenance

The immediate conceptual baseline is the six-operator `FM7` UGen distributed
by sc3-plugins. FMMatrixUGens independently implements a new arbitrary-size
architecture and does not copy `FM7` source code, DX firmware, presets, or
algorithm tables. See [ORIGINS.md](ORIGINS.md) and
[notes/RECONNAISSANCE.md](notes/RECONNAISSANCE.md).

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).
