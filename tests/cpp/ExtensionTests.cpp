// SPDX-License-Identifier: GPL-3.0-or-later
#include "MatrixEngine.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

void require(bool ok, const char* message)
{
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

void exponentialFM()
{
    for (const double depth : {-1.0, 0.0, 1.0, 2.0}) {
        std::vector<double> inputs{2, -100, 0, 0.2, fmmatrix::tau/4, 0.7, 1, 0, depth, 0, 0};
        std::vector<double> memory(fmmatrix::MatrixEngine::memorySize(2, 1));
        fmmatrix::MatrixEngine engine(2, 1, memory.data());
        engine.initialize([&](std::size_t i) { return inputs[i]; });
        for (int frame = 0; frame < 4096; ++frame) {
            engine.tick<fmmatrix::Law::ExpFM>([&](std::size_t i, unsigned, unsigned) { return inputs[i]; }, 48000);
            const double cycles = frame == 0 ? 0 : (-100.0/48000) * (1 + (frame-1)*std::exp2(depth));
            require(std::abs(engine.output[0] - 0.7*std::sin(0.2 + fmmatrix::tau*cycles)) < 1e-12,
                "octave FM and negative carrier recurrence");
        }
        inputs[1] = 0;
        inputs[8] = std::numeric_limits<float>::max();
        engine.tick<fmmatrix::Law::ExpFM>([&](std::size_t i, unsigned, unsigned) { return inputs[i]; }, 48000);
        require(std::isfinite(engine.output[0]) && std::isfinite(engine.dsp.phase[0]), "zero frequency and extreme octave depth remain finite");
        inputs[1] = std::numeric_limits<float>::max();
        engine.tick<fmmatrix::Law::ExpFM>([&](std::size_t i, unsigned, unsigned) { return inputs[i]; }, 48000);
        require(std::isfinite(engine.dsp.phase[0]), "bounded exponential-FM arithmetic");
    }
}

void waveAndExternal()
{
    constexpr std::size_t frames = 8192;
    std::vector<float> sine(frames), cosine(frames);
    for (std::size_t i = 0; i < frames; ++i) {
        sine[i] = static_cast<float>(std::sin(fmmatrix::tau*static_cast<double>(i)/frames));
        cosine[i] = static_cast<float>(std::cos(fmmatrix::tau*static_cast<double>(i)/frames));
    }
    require(fmmatrix::cycleTable(nullptr, frames, 0) == 0 && fmmatrix::cycleTable(sine.data(), 1, 0) == 0,
        "missing/too short tables are silent");
    for (int i = -10000; i < 10000; ++i) {
        const double phase = i*0.013;
        require(std::abs(fmmatrix::cycleTable(sine.data(), frames, phase)-std::sin(phase)) < 1.1e-7,
            "cyclic lookup, negative phases and interpolation");
    }
    for (unsigned factor : {1U, 2U, 4U, 8U}) {
        std::vector<double> inputs{2, 123, -234, 0.1, 0.2, 0.7, 0.8, 0.1, 0.2, 0.3, 0.1};
        std::vector<double> a(fmmatrix::MatrixEngine::memorySize(2, factor)), b(a.size());
        fmmatrix::MatrixEngine wave(2, factor, a.data()), reference(2, factor, b.data());
        wave.initialize([&](std::size_t i) { return inputs[i]; });
        reference.initialize([&](std::size_t i) { return inputs[i]; });
        auto read = [&](std::size_t i, unsigned, unsigned) { return inputs[i]; };
        for (int frame = 0; frame < 1024; ++frame) {
            wave.tickExtended(read, [&](std::size_t i, double p, double m) {
                return fmmatrix::cycleTable(i == 0 ? sine.data() : cosine.data(), frames, p+m);
            }, 7, 0, 0, 48000);
            reference.tickExtended(read, [](std::size_t i, double p, double m) {
                return i == 0 ? std::sin(p+m) : std::cos(p+m);
            }, 7, 0, 0, 48000);
            for (std::size_t i = 0; i < 2; ++i) require(std::abs(wave.output[i]-reference.output[i]) < 2e-7,
                "multiple waveforms, negative frequency and oversampling");
        }
    }
    // N=2, K=2; external columns are current input, not delayed oscillator state.
    std::vector<double> inputs{2, 123, -234, 0.1, 0.2, 0.7, 0.8, 2, 0, 0,
        0.1, 0.2, 0.3, -0.4, 0.2, 0.1, -0.3, 0.4};
    std::vector<double> memory(fmmatrix::MatrixEngine::memorySize(2, 1));
    fmmatrix::MatrixEngine engine(2, 1, memory.data());
    engine.initialize([&](std::size_t i) { return inputs[i]; });
    double old[2]{0, 0};
    for (int frame = 0; frame < 2048; ++frame) {
        inputs[8] = std::sin(frame*0.13); inputs[9] = std::cos(frame*0.023);
        engine.tickExtended([&](std::size_t i, unsigned, unsigned) { return inputs[i]; },
            [](std::size_t, double p, double m) { return std::sin(p+m); }, 10, 2, 8, 48000);
        double expected[2];
        for (std::size_t i = 0; i < 2; ++i) {
            const auto row = 10+4*i;
            const double sum = inputs[row]*old[0] + inputs[row+1]*old[1] + inputs[row+2]*inputs[8] + inputs[row+3]*inputs[9];
            expected[i] = inputs[5+i] * std::sin(inputs[3+i] + fmmatrix::tau*frame*inputs[1+i]/48000 + sum);
            require(std::abs(engine.output[i]-expected[i]) < 2e-12, "rectangular external-input recurrence");
        }
        std::copy(expected, expected+2, old);
    }
}

void nonlinearAndDelays()
{
    const double expected[] = {2.5, std::tanh(2.5), 2.5/3.5, -0.5, 0.5, 2.5, 6.25, 1};
    for (unsigned shape = 0; shape < 8; ++shape) {
        require(std::abs(fmmatrix::nonlinear(2.5, shape)-expected[shape]) < 1e-14, "nonlinear shape definition");
        require(std::isfinite(fmmatrix::nonlinear(std::numeric_limits<double>::infinity(), shape)), "nonfinite shape input");
        std::vector<double> input{1, -200, 0.2, 0.7, 0.2};
        std::vector<double> memory(fmmatrix::MatrixEngine::memorySize(1, 1));
        fmmatrix::MatrixEngine engine(1, 1, memory.data());
        engine.initialize([&](std::size_t i) { return input[i]; });
        double old = 0;
        for (int frame = 0; frame < 512; ++frame) {
            engine.tickExtended([&](std::size_t i, unsigned, unsigned) { return input[i]; },
                [&](std::size_t, double p, double m) { return std::sin(p + fmmatrix::nonlinear(m, shape)); }, 4, 0, 0, 48000);
            old = 0.7*std::sin(0.2 + fmmatrix::tau*(-200)*frame/48000 + fmmatrix::nonlinear(0.2*old, shape));
            // Discontinuous nonlinearities are deliberately tested below chaos-strength feedback.
            require(std::abs(engine.output[0]-old) < 1e-10, "per-destination nonlinear recurrence");
        }
    }
    for (unsigned factor : {1U, 2U, 4U, 8U}) {
        std::vector<double> input{2, 123, -234, 0.1, 0.2, 0.7, 0.8, 0.2, 0.3, 0.0, 0.0};
        const std::uint32_t sources[]{0, 1}, dest[]{1, 0};
        std::vector<double> a(fmmatrix::MatrixEngine::memorySize(2, factor)), b(a.size()), ring(2*(factor+1));
        fmmatrix::MatrixEngine delayed(2, factor, a.data()), graph(2, factor, b.data());
        fmmatrix::DelayHistory history(ring.data(), 2, factor+1, factor);
        history.reset();
        delayed.initialize([&](std::size_t i) { return input[i]; });
        graph.initialize([&](std::size_t i) { return input[i]; });
        for (int frame = 0; frame < 1024; ++frame) {
            auto read = [&](std::size_t i, unsigned, unsigned) { return input[i]; };
            delayed.tickDelayedGraph(read, sources, dest, 2, 7, 9, history, 48000);
            graph.tickGraph(read, sources, dest, 2, 7, 48000);
            for (std::size_t i = 0; i < 2; ++i) require(delayed.output[i] == graph.output[i], "minimum delay equals one HOST sample at all factors");
        }
    }
    std::vector<double> ring(18), memory(fmmatrix::MatrixEngine::memorySize(2, 1));
    fmmatrix::DelayHistory history(ring.data(), 2, 9, 7.5);
    history.reset();
    fmmatrix::MatrixEngine engine(2, 1, memory.data());
    std::vector<double> input{2, 123, -234, 0.1, 0.2, 0.7, 0.8, 0.2, 0.3, 0, 0};
    const std::uint32_t sources[]{0, 1}, dest[]{1, 0};
    engine.initialize([&](std::size_t i) { return input[i]; });
    std::vector<std::vector<double>> past(2, std::vector<double>(2048, 0));
    for (int frame = 0; frame < 2048; ++frame) {
        const double delay[2] = {1.25 + 2*(1+std::sin(frame*0.013)), 7.5};
        input[9] = delay[0]/48000; input[10] = 20.0/48000; // clamp to 7.5
        engine.tickDelayedGraph([&](std::size_t i, unsigned, unsigned) { return input[i]; }, sources, dest, 2, 7, 9, history, 48000);
        for (int edge = 0; edge < 2; ++edge) {
            const int whole = static_cast<int>(delay[edge]);
            const double fraction = delay[edge]-whole;
            const double a = frame >= whole ? past[sources[edge]][static_cast<std::size_t>(frame-whole)] : 0;
            const double b = frame > whole ? past[sources[edge]][static_cast<std::size_t>(frame-whole-1)] : 0;
            const auto i = dest[edge];
            past[i][static_cast<std::size_t>(frame)] = input[5+i] * std::sin(input[3+i] + fmmatrix::tau*frame*input[1+i]/48000
                + input[7+static_cast<std::size_t>(edge)] * (a*(1-fraction)+b*fraction));
            require(std::abs(engine.output[i]-past[i][static_cast<std::size_t>(frame)]) < 2e-12, "fractional/dynamic delay, wraparound, maximum clamp recurrence");
        }
    }
}

int main()
{
    exponentialFM();
    waveAndExternal();
    nonlinearAndDelays();
    std::cout << "FMMatrixUGens extension tests passed\n";
}
