# Mapping from FM7

FM7's conceptual matrix orientation is retained: row is destination and column
is source. Six frequency/phase/amplitude operator descriptions become three
six-element vectors; retain the 6x6 matrix and select or mix the six outputs
in the language. PM depth remains radians per unit source amplitude.

The new PM network explicitly reads every source from the preceding complete
network vector. All cycles and self-edges therefore have the same one-host-
sample delay, regardless of operator ordering. Do not expect sample-identical
results from FM7 algorithms or feedback patches: state traversal, phase
precision, initial-phase handling and control interpolation can differ.

Independent operators have been compared with SinOsc at 44.1 and 48 kHz. A
single PM edge is checked against SinOsc with an explicit Delay1 source. Native
recurrence tests cover self-feedback and cyclic graphs independently of traversal
order. These are mathematical compatibility tests, not a claim that FM7 factory
algorithms, presets or undocumented implementation quirks were reproduced.

The optional `tests/sc/fm7_comparison.scd` also renders the installed FM7 itself
against PMMatrix at 1x/2x and PMMatrixZDF. It covers six independent operators,
one-way modulation, self-feedback and two hand-built three-operator stacks.
No algorithm tables are copied. Results are in `benchmarks/fm7_comparison.csv`.

In these 48 kHz, 0.2-second, modest-coupling cases, raw 1x differences were
about 0.018 RMS. Advancing PMMatrix by one sample reduces differences to about
8.9e-6 RMS, with maximum error below 4.6e-5. This measured alignment explains
most of the startup difference for these cases; it does not establish general
sample identity under dynamic controls or strong feedback. Unaligned 2x renders
include the output filter's latency, so their larger difference is not an alias
metric. ZDF solves a different equation and is not a compatibility mode.

FMMatrix is a different model, using Hz deviations inside phase integration.
PMMatrixZDF is different again: same-sample implicit coupling, approximated by
iteration. Changing class names without converting units or considering the
equation is not a transparent migration. Oversampling also adds output-filter
latency and changes the discretization of the oscillator dynamics.
