# Origins

| Component | Source or technique | Implementation relationship |
|---|---|---|
| `PMMatrix`, `PMGraph` | sc3-plugins FM7 matrix-operator concept | Independent arbitrary-size synchronous PM and sparse traversal |
| `FMMatrix`, `FMMatrixExp` | Frequency integration and octave-to-ratio conversion | Independent linear-Hz and exponential-octave laws |
| `PMMatrixZDF` | Damped Jacobi fixed-point iteration of the implicit PM equation | Independent finite-budget solver with residual/contraction tests |
| `PMMatrixWave` | Cyclic table lookup with linear interpolation | Independent raw mono buffer reader using the SC buffer API |
| `PMMatrixIn` | Rectangular coupling matrix with external source columns | Independent routing of delayed internal and current external sources |
| `PMDelayGraph` | Fractional delay with per-source ring buffers | Independent sparse delayed PM with linear interpolation |
| `PMMatrixNL` | Standard scalar nonlinear functions | Independent transforms of destination sums; formulas in class help |
| Oversampling | Windowed-sinc half-band FIR, four-term Blackman-Harris window | Independent filter and cascade; no SciPy dependency |
| Language/server integration | SuperCollider MultiOutUGen and server-plugin API | Independent wrappers and memory lifecycle |

The immediate conceptual source is sc3-plugins
[FM7](https://doc.sccode.org/Classes/FM7.html), including its
[server implementation](https://github.com/supercollider/sc3-plugins/blob/main/source/SkUGens/FM7.cpp).
Its six-operator destination-row/source-column convention informs this project's
arbitrary-size architecture. The baseline was inspected, but no upstream DSP
code, DX firmware, algorithm tables, presets or factory waveforms were copied.
FM7 is used only by an optional comparison test; it is not a runtime dependency.

The [SciPy window reference](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.windows.blackmanharris.html)
documents the standard symmetric four-term window used for filter design.
The [SuperCollider project](https://github.com/supercollider/supercollider) supplies
the required SDK and host runtime. SDK files are not bundled in binary releases.

See [implementation equations](docs/IMPLEMENTATION.md),
[reconnaissance](notes/RECONNAISSANCE.md), and
[FM7 migration measurements](notes/FM7_MIGRATION.md) for behavior changes.
