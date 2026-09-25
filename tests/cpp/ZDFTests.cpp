// SPDX-License-Identifier: GPL-3.0-or-later
#include "MatrixEngine.hpp"
#include <cstdlib>
#include <iostream>
#include <vector>

void require(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

int main()
{
    for (const double coupling : {-0.8, -0.2, 0.0, 0.2, 0.8}) {
        std::vector<double> memory(fmmatrix::MatrixEngine::memorySize(1, 1));
        std::vector<double> inputs{1, 0, 0.3, 1, coupling};
        fmmatrix::MatrixEngine engine(1, 1, memory.data());
        engine.initialize([&](std::size_t i) { return inputs[i]; });
        engine.tick<fmmatrix::Law::ZDF>([&](std::size_t i, unsigned, unsigned) { return inputs[i]; }, 48000, 64);
        // Independent bracketed root reference. |coupling| < 1 makes y-F(y)
        // strictly increasing, so this root is unique on [-1,1].
        double low = -1, high = 1;
        for (int iteration = 0; iteration < 100; ++iteration) {
            const double middle = (low + high) * 0.5;
            if (middle - std::sin(0.3 + coupling * middle) > 0) high = middle;
            else low = middle;
        }
        const double root = (low + high) * 0.5;
        // Fixed-point convergence is bounded by |k|^iterations. At k=-0.8,
        // 64 iterations are not necessarily enough for an arbitrary 1e-8
        // startup tolerance; verify the actual contraction bound instead.
        require(std::abs(engine.output[0] - root) <= std::pow(std::abs(coupling), 64) * std::abs(root) + 1e-14,
            "ZDF startup satisfies contraction bound against bisection root");
        engine.tick<fmmatrix::Law::ZDF>([&](std::size_t i, unsigned, unsigned) { return inputs[i]; }, 48000, 64);
        require(std::abs(engine.output[0] - root) < 2e-12, "stationary ZDF matches independent bisection root");
    }
    for (const unsigned factor : {1U, 2U, 4U, 8U}) {
        std::vector<double> memory(fmmatrix::MatrixEngine::memorySize(2, factor));
        std::vector<double> inputs{2, 0, 0, fmmatrix::tau/4, 0, 1, 1, 0, 0, 1, 0};
        fmmatrix::MatrixEngine engine(2, factor, memory.data());
        engine.initialize([&](std::size_t i) { return inputs[i]; });
        engine.tick<fmmatrix::Law::ZDF>([&](std::size_t i, unsigned, unsigned) { return inputs[i]; }, 48000, 2);
        require(std::abs(engine.dsp.output[1] - std::sin(1.0)) < 1e-13, "ZDF source couples within the first sample");
    }
    for (const double coupling : {0.2, 0.6, 1.4, 10.0, 1e30}) {
        std::vector<double> memory(fmmatrix::MatrixEngine::memorySize(2, 1));
        std::vector<double> inputs{2, 110, -220, 0.3, -0.2, 0.8, 0.7, coupling, -coupling, coupling, coupling};
        fmmatrix::MatrixEngine engine(2, 1, memory.data());
        engine.initialize([&](std::size_t i) { return inputs[i]; });
        for (int frame = 0; frame < 2048; ++frame) {
            const double phase[]{engine.dsp.phase[0], engine.dsp.phase[1]};
            engine.tick<fmmatrix::Law::ZDF>([&](std::size_t i, unsigned, unsigned) { return inputs[i]; }, 48000, 64, 0.7);
            for (std::size_t i = 0; i < 2; ++i) {
                require(std::isfinite(engine.output[i]) && std::abs(engine.output[i]) <= 1, "bounded finite ZDF for strong coupling");
                if (coupling == 0.2) {
                    const double residual = engine.output[i] - inputs[5+i] * std::sin(phase[i]
                        + inputs[7+2*i] * engine.output[0] + inputs[8+2*i] * engine.output[1]);
                    require(std::abs(residual) < 1e-12, "contractive mutual ZDF residual");
                }
            }
        }
    }
    std::cout << "FMMatrixUGens ZDF tests passed\n";
}
