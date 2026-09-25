(
var pages = ["Classes/PMMatrix", "Classes/FMMatrix", "Classes/PMMatrixZDF", "Classes/PMGraph", "Classes/FMMatrixExp", "Classes/PMMatrixWave", "Classes/PMMatrixIn", "Classes/PMDelayGraph", "Classes/PMMatrixNL", "Guides/FMMatrixUGens"];
var ok = true;
SCDoc.verbosity = 2;
SCDoc.indexAllDocuments(true);
pages.do { |path|
    var document = SCDoc.documents[path];
    if(document.isNil) { ("MISSING_DOC " ++ path).postln; ok = false } {
        var root = SCDoc.parseDoc(document);
        if(root.isNil) { ("PARSE_FAILED " ++ path).postln; ok = false } {
            SCDoc.renderer.renderToFile(document.destPath, document, root);
            ("RENDERED " ++ path).postln
        }
    }
};
if(ok) { "FMMATRIX_SCDOC_OK".postln; 0.exit } {
    "FMMATRIX_SCDOC_FAILED".postln; 1.exit
};
)
