(
// Moderate coupling: compare delayed PM with eight fixed-point iterations.
// The two models have different equations, even when both sound stable.
s.waitForBoot {
    x = {
        var matrix = [[0.2, 0.3], [0.3, -0.2]];
        var delayed = PMMatrix.ar([110, 173], 0.2, 0.7, matrix)[0];
        var implicit = PMMatrixZDF.ar([110, 173], 0.2, 0.7, matrix,
            iterations: 8, damping: 1)[0];
        [delayed, implicit] * 0.12 * Line.kr(0, 1, 0.05)
    }.play;
};
)
