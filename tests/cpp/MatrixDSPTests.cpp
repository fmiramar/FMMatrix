// SPDX-License-Identifier: GPL-3.0-or-later
#include "MatrixDSP.hpp"
#include "ControlSmoothing.hpp"
#include "MatrixEngine.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace {
void require(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

template <fmmatrix::Law law> void independentOscillators()
{
    for (const std::size_t n : {1U, 2U, 3U, 6U, 8U, 16U, 31U, 64U}) {
        std::vector<double> storage(3 * n), inputs(1 + 3 * n + n * n);
        for (std::size_t i = 0; i < n; ++i) {
            inputs[1 + i] = (i % 2 == 0 ? 1.0 : -1.0) * (101.0 + 13.0 * static_cast<double>(i));
            inputs[1 + n + i] = 0.1 * static_cast<double>(i);
            inputs[1 + 2 * n + i] = 0.5;
        }
        auto read = [&](std::size_t index) { return inputs[index]; };
        fmmatrix::MatrixDSP dsp(n, storage.data());
        dsp.initialize(read);
        for (int frame = 0; frame < 96000; ++frame) {
            dsp.tick<law>(read, 48000.0);
            for (std::size_t i = 0; i < n; ++i) {
                const double expected = 0.5 * std::sin(inputs[1 + n + i]
                    + fmmatrix::tau * inputs[1 + i] * frame / 48000.0);
                require(std::abs(expected - dsp.output[i]) < 2e-10, "independent signed-frequency reference");
                require(std::abs(dsp.phase[i]) <= fmmatrix::tau / 2, "bounded phase");
            }
        }
    }
    require(fmmatrix::wrap(std::numeric_limits<double>::infinity()) == 0.0, "infinite phase guard");
    require(std::isfinite(fmmatrix::wrap(1e300)), "large finite phase");
}

void synchronousPM()
{
    constexpr std::size_t n = 3;
    std::vector<double> inputs{3, 101, -233, 397, 0.2, -0.5, 1.1, 0.8, 0.7, -0.6,
        0.3, 0.0, 1.7, 0.9, -0.1, 0.0, 0.0, 1.3, 0.2};
    std::vector<double> state(3 * n), permutedState(3 * n), permutation(inputs.size());
    const std::size_t order[]{2, 0, 1};
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t v = 0; v < 3; ++v) permutation[1 + v * n + i] = inputs[1 + v * n + order[i]];
        for (std::size_t j = 0; j < n; ++j) permutation[1 + 3 * n + i * n + j] = inputs[1 + 3 * n + order[i] * n + order[j]];
    }
    auto read = [&](std::size_t i) { return inputs[i]; };
    auto shuffled = [&](std::size_t i) { return permutation[i]; };
    fmmatrix::MatrixDSP dsp(n, state.data()), other(n, permutedState.data());
    dsp.initialize(read); other.initialize(shuffled);
    double old[]{0, 0, 0}, phase[]{0.2, -0.5, 1.1}, expected[n]{};
    for (int frame = 0; frame < 48000; ++frame) {
        // Deliberately traverse backwards in the independent reference.
        for (std::size_t i = n; i-- > 0;) {
            double sum = 0;
            for (std::size_t j = 0; j < n; ++j) sum += inputs[1 + 3 * n + i * n + j] * old[j];
            expected[i] = inputs[1 + 2 * n + i] * std::sin(phase[i] + sum);
            phase[i] = fmmatrix::wrap(phase[i] + fmmatrix::tau * inputs[1 + i] / 48000.0);
        }
        dsp.tick(read, 48000); other.tick(shuffled, 48000);
        for (std::size_t i = 0; i < n; ++i) {
            require(std::abs(dsp.output[i] - expected[i]) < 2e-13, "PM recurrence / synchronous cyclic and self feedback");
            require(std::abs(other.output[i] - dsp.output[order[i]]) < 2e-13, "operator permutation invariance");
            old[i] = expected[i];
        }
    }
}

void linearFM()
{
    std::vector<double> inputs{2, -301, 127, 0.3, -0.7, 0.8, -0.6, 217, -971, 481, -67};
    std::vector<double> state(6), pmState(6);
    fmmatrix::MatrixDSP dsp(2, state.data()), pm(2, pmState.data());
    auto read = [&](std::size_t i) { return inputs[i]; };
    dsp.initialize(read); pm.initialize(read);
    double old[]{0, 0}, phase[]{0.3, -0.7};
    double difference = 0;
    for (int frame = 0; frame < 480000; ++frame) {
        double next[2]{};
        for (std::size_t i = 0; i < 2; ++i) {
            next[i] = inputs[5 + i] * std::sin(phase[i]);
            phase[i] = std::remainder(phase[i] + fmmatrix::tau / 48000
                * (inputs[1 + i] + inputs[7 + 2 * i] * old[0] + inputs[8 + 2 * i] * old[1]), fmmatrix::tau);
        }
        dsp.tick<fmmatrix::Law::FM>(read, 48000); pm.tick(read, 48000);
        for (std::size_t i = 0; i < 2; ++i) {
            require(std::abs(dsp.output[i] - next[i]) < 2e-9, "linear FM recurrence with negative deviation and base frequency");
            require(std::abs(dsp.phase[i]) <= fmmatrix::tau / 2, "long-running FM phase remains bounded");
            old[i] = next[i];
        }
        difference += std::abs(dsp.output[0] - pm.output[0]);
    }
    require(difference > 100, "Hz modulation must differ from radians modulation");
    for (std::size_t i = 1; i < inputs.size(); ++i) inputs[i] = std::numeric_limits<float>::max();
    dsp.initialize(read);
    for (int frame = 0; frame < 1000; ++frame) {
        dsp.tick<fmmatrix::Law::FM>(read, 48000);
        require(std::isfinite(dsp.output[0]) && std::isfinite(dsp.phase[1]), "extreme finite FM values remain finite");
    }
    inputs[1] = std::numeric_limits<double>::quiet_NaN();
    inputs[7] = std::numeric_limits<double>::infinity();
    dsp.tick<fmmatrix::Law::FM>(read, 48000);
    require(std::isfinite(dsp.output[0]) && std::isfinite(dsp.phase[1]), "nonfinite input guard");
}
} // namespace

int main()
{
    independentOscillators<fmmatrix::Law::PM>();
    independentOscillators<fmmatrix::Law::FM>();
    synchronousPM();
    linearFM();
    double phase = 0.2, trigger = 0;
    fmmatrix::resetPhase(phase, trigger, 1, 0.7);
    require(phase == 0.7, "rising edge reset occurs before output");
    fmmatrix::resetPhase(phase, trigger, 1, -0.3);
    require(phase == 0.7, "held trigger does not reset repeatedly");
    fmmatrix::resetPhase(phase, trigger, 0, -0.3);
    fmmatrix::resetPhase(phase, trigger, 1, -0.3);
    require(phase == -0.3, "reset re-arms after nonpositive trigger");
    require(fmmatrix::ramp(0, 8, 0.5) == 4, "control block midpoint");
    double smoothed = 0;
    for (int i = 0; i < 480; ++i) smoothed = fmmatrix::lag(smoothed, 1, fmmatrix::lagAmount(0.01, 48000));
    require(std::abs(smoothed - (1 - std::exp(-1))) < 1e-13, "documented matrix smoothing time constant");
    for (const unsigned factor : {1U, 2U, 4U, 8U}) {
        std::vector<double> memory(fmmatrix::MatrixEngine::memorySize(2, factor));
        std::vector<double> inputs{2, 0, 0, fmmatrix::tau/4, 0, 1, 1, 0, 0, 1, 0};
        fmmatrix::MatrixEngine engine(2, factor, memory.data());
        engine.initialize([&](std::size_t i) { return inputs[i]; });
        auto read = [&](std::size_t i, unsigned, unsigned) { return inputs[i]; };
        engine.tick<fmmatrix::Law::PM>(read, 48000);
        require(engine.dsp.output[1] == 0, "oversampled PM edges retain a full host-sample delay");
        engine.tick<fmmatrix::Law::PM>(read, 48000);
        require(std::abs(engine.dsp.output[1] - std::sin(1.0)) < 1e-14, "oversampled PM delayed source arrives next host sample");
        for (int i = 0; i < 256; ++i) engine.tick<fmmatrix::Law::PM>(read, 48000);
        require(std::abs(engine.output[1] - std::sin(1.0)) < 1e-12, "oversampled DC amplitude / filter settling");
    }
    std::cout << "FMMatrixUGens DSP tests passed\n";
}
