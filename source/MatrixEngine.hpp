// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "Decimator.hpp"
#include "ExtendedDSP.hpp"

namespace fmmatrix {

struct MatrixEngine {
    std::size_t n;
    unsigned factor;
    MatrixDSP dsp;
    double *history, *scratch, *output;
    Decimator decimator;

    static std::size_t memorySize(std::size_t count, unsigned rate) noexcept
    {
        return (5 + rate) * count + Decimator::memorySize(count, rate);
    }
    MatrixEngine(std::size_t count, unsigned rate, double* memory) noexcept
        : n(count), factor(rate), dsp(count, memory), history(memory + 3 * count),
          scratch(history + rate * count), output(scratch + count),
          decimator(count, rate, output + count) {}

    template <typename Read> void initialize(Read read) noexcept
    {
        dsp.previous = dsp.phase + n;
        dsp.initialize(read);
        std::fill(history, output + n, 0.0);
        decimator.reset();
    }

    template <Law law, typename Read> void tick(Read read, double sampleRate,
                                              unsigned iterations = 4, double damping = 1) noexcept
    {
        for (unsigned sub = 0; sub < factor; ++sub) {
            // One separate history vector per substep preserves an R-step
            // delay at R*fs, i.e. exactly one host sample, on every edge.
            if constexpr (law == Law::ZDF) dsp.previous = dsp.phase + n;
            else dsp.previous = history + static_cast<std::size_t>(sub) * n;
            dsp.tick<law>([&](std::size_t index) { return read(index, sub, factor); }, sampleRate * factor, iterations, damping);
            std::copy(dsp.output, dsp.output + n, scratch);
            if (decimator.push(scratch)) std::copy(scratch, scratch + n, output);
        }
    }

    template <typename Read> void tickGraph(Read read, const std::uint32_t* sources,
        const std::uint32_t* destinations, std::size_t edges, std::size_t depthStart, double sampleRate) noexcept
    {
        for (unsigned sub = 0; sub < factor; ++sub) {
            dsp.previous = history + static_cast<std::size_t>(sub) * n;
            dsp.tickGraph([&](std::size_t i) { return read(i, sub, factor); },
                sources, destinations, edges, depthStart, sampleRate * factor);
            std::copy(dsp.output, dsp.output+n, scratch);
            if (decimator.push(scratch)) std::copy(scratch, scratch+n, output);
        }
    }

    template <typename Read, typename Evaluate> void tickExtended(Read read, Evaluate evaluate,
        std::size_t depthStart, std::size_t externalCount, std::size_t externalStart, double sampleRate) noexcept
    {
        for (unsigned sub = 0; sub < factor; ++sub) {
            dsp.previous = history + static_cast<std::size_t>(sub)*n;
            dsp.tickExtended([&](std::size_t i) { return read(i, sub, factor); }, evaluate,
                depthStart, externalCount, externalStart, sampleRate*factor);
            std::copy(dsp.output, dsp.output+n, scratch);
            if (decimator.push(scratch)) std::copy(scratch, scratch+n, output);
        }
    }

    template <typename Read> void tickDelayedGraph(Read read, const std::uint32_t* sources,
        const std::uint32_t* destinations, std::size_t edges, std::size_t depthStart,
        std::size_t delayStart, DelayHistory& delays, double sampleRate) noexcept
    {
        for (unsigned sub = 0; sub < factor; ++sub) {
            std::fill(dsp.output, dsp.output+n, 0);
            for (std::size_t e = 0; e < edges; ++e)
                dsp.output[destinations[e]] += finite(read(depthStart+e, sub, factor))
                    * delays.read(sources[e], read(delayStart+e, sub, factor)*sampleRate*factor, factor);
            for (std::size_t i = 0; i < n; ++i) {
                dsp.output[i] = finite(read(1+2*n+i, sub, factor)) * std::sin(wrap(dsp.phase[i] + dsp.output[i]));
                dsp.phase[i] = wrap(dsp.phase[i] + tau * finite(read(1+i, sub, factor)) / (sampleRate*factor));
            }
            delays.push(dsp.output);
            std::copy(dsp.output, dsp.output+n, scratch);
            if (decimator.push(scratch)) std::copy(scratch, scratch+n, output);
        }
    }
};

} // namespace fmmatrix
