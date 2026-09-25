# Milestone A reconnaissance

## Baseline inspected

- sc3-plugins `source/SkUGens/FM7.cpp`, `source/SkUGens/sc/FM7.sc`, and the
  `FM7` help file: six fixed operators, a destination-row/source-column `6 x 6`
  phase matrix, one output per operator, and a language wrapper that flattens
  control and matrix arrays before `multiNewList`.
- SuperCollider `UGen.sc` and `MultiOutUGen.schelp`: `multiNewList` performs
  multichannel expansion only when an input remains an Array, and output count
  is fixed when the SynthDef is compiled. This project therefore explicitly
  flattens all operator vectors and matrix rows before construction.
- SuperCollider server-plugin API and the installed C++17-compatible project
  conventions: constructor-time `RTAlloc` / destructor `RTFree`, `SETCALC`,
  `DefineDtorUnit`, and no allocation in the calculation callback.
- SuperCollider example-plugin guidance: a plugin must build against matching
  SuperCollider source headers; scsynth and supernova binaries are separate
  targets.
- Existing BiroSCa projects `NovelOscUGens`, `LPC2UGens`, and
  `AudioRestoration`: standalone CMake C++17 projects with explicit server
  targets, native tests, language tests, SCDoc verification, and local
  extension installation.

## Resulting Milestone A decisions

1. The server input prefix is `N`, followed by `freqs[N]`, `phases[N]`,
   `amps[N]`, and row-major `matrix[N*N]`.
2. `N` is both a scalar structural input and the number of `MultiOutUGen`
   outputs. The server checks the exact input count, so malformed hand-authored
   SynthDefs cannot overrun layout memory.
3. There is no small artistic operator ceiling. The only limits are representable
   SynthDef counts, real-time allocation, and later dense CPU cost.
4. Milestone A owns a per-operator double array solely to exercise lifecycle
   behavior. It deliberately clears outputs until the independent-oscillator
   reference tests of Milestone B are ready.
5. `FM7` is not modified and no upstream implementation text was reused.

## References

- `FM7` help: <https://doc.sccode.org/Classes/FM7.html>
- sc3-plugins: <https://github.com/supercollider/sc3-plugins>
- SuperCollider plugin examples: <https://github.com/supercollider/example-plugins>
