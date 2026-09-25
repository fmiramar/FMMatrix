(
var project = thisProcess.nowExecutingPath.dirname.dirname.dirname;
var server = Server(\matrixExamples, NetAddr("127.0.0.1", 57138));
var oldDefault = Server.default, failed = false, started = Set.new;
var cases = List.new;
(project +/+ "examples/*.scd").pathMatch.sort.do { |path|
    cases.add([PathName(path).fileName, File.readAllString(path)])
};
// Exercise each topology variant of the shared graph example.
[\lattice, \random, \clusters].do { |topology|
    var source = File.readAllString(project +/+ "examples/02_sparse_graphs.scd");
    cases.add([topology.asString, source.replace("topology = \\ring", "topology = \\" ++ topology)])
};
((project +/+ "HelpSource/Classes/*.schelp").pathMatch ++ (project +/+ "HelpSource/Guides/*.schelp").pathMatch).do { |path|
    var source = File.readAllString(path);
    (source.findAll("code::\n") ? []).do { |start|
        var stop = source.find("\n::", offset: start+7);
        var code = source.copyRange(start+7, stop-1);
        if(code.contains(".play") or: { code.contains("Synth(") }) {
            cases.add([PathName(path).fileName, code])
        }
    }
};
Server.scsynth;
server.options.numInputBusChannels = 0; server.options.numOutputBusChannels = 2;
server.options.memSize = 262144; server.options.loadDefs = false;
server.options.ugenPluginsPath = [Platform.resourceDir +/+ "plugins", Platform.userExtensionDir +/+ "FMMatrixUGens"];
SystemClock.sched(90, { "FMMATRIX_EXAMPLES_TIMEOUT".postln; server.quit; 1.exit; nil });
server.waitForBoot({
    Routine {
        try {
            Server.default = server; s = server; server.mute;
            OSCdef(\exampleFailures, { |message| message.postln; failed = true }, '/fail', server.addr);
            OSCdef(\exampleStarted, { |message| started.add(message[1]) }, '/n_go', server.addr);
            server.sync;
            cases.do { |entry|
                var deadline;
                x = nil; b = nil;
                entry[1].interpret;
                deadline = Main.elapsedTime + 5;
                while { x.isNil and: { Main.elapsedTime < deadline } } { 0.05.wait };
                server.sync; 0.1.wait;
                if(x.isNil or: { started.includes(x.nodeID).not } or: { failed }) {
                    Error("Example did not start: %".format(entry[0])).throw
                };
                "EXAMPLE_OK %".format(entry[0]).postln;
                x.free; server.sync;
                if(b.notNil) { b.asArray.do(_.free); server.sync };
            };
            OSCdef(\exampleFailures).free; OSCdef(\exampleStarted).free;
            server.quit; Server.default = oldDefault; s = oldDefault;
            "FMMATRIX_EXAMPLES_OK".postln; 0.exit
        } { |error|
            error.reportError; server.quit; "FMMATRIX_EXAMPLES_FAILED".postln; 1.exit
        }
    }.play(SystemClock)
}, 20, { "FMMATRIX_EXAMPLES_BOOT_FAILED".postln; 1.exit });
)
