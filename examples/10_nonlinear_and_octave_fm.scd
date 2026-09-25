(
s.waitForBoot {
    x = {
        var nonlinear = PMMatrixNL.ar([110, 173], 0, [1, 0.7],
            [[0.1, 2.5], [0.3, 0]], [1, 3], oversample: 4)[0];
        var exponential = FMMatrixExp.ar([110, 73], 0, [1, 0.7],
            [[0, 0.7], [0.1, 0]], oversample: 4)[0];
        [nonlinear, exponential] * 0.1 * Line.kr(0, 1, 0.05)
    }.play;
};
)
