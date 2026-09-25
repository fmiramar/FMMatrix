(
// Select a topology before building the Synth: ring, lattice, random, clusters.
// For n = 64 or more, set s.options.numWireBufs = 1024 before booting.
var n = 16, topology = \ring, sources = List.new, destinations = List.new;
var edge = { |source, destination| sources.add(source); destinations.add(destination) };
thisThread.randSeed = 1977;
switch(topology,
    \ring, { n.do { |i| edge.(i, (i+1)%n) } },
    \lattice, { n.do { |i| edge.(i, (i+1)%n); edge.(i, (i+8)%n) } },
    \random, { (n*2).do { edge.(n.rand, n.rand) } },
    \clusters, { n.do { |i| 4.do { |j| edge.(i, (i div: 8)*8 + ((i+j+1)%8)) }; if(i%8 == 0) { edge.(i, (i+8)%n) } } }
);
s.waitForBoot {
    x = {
        var ops = PMGraph.ar(Array.series(n, 80, 3), 0, 0.7,
            sources.asArray, destinations.asArray, SinOsc.kr(0.07).range(0.05, 0.5), smooth: 0.03);
        Splay.ar(ops) * 0.08 * Line.kr(0, 1, 0.05)
    }.play;
};
)
