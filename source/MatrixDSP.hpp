// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace fmmatrix {

constexpr double tau = 6.283185307179586476925286766559;
enum class Law { PM, FM, ZDF, Graph, ExpFM, Wave, In, DelayGraph, NL };

inline double finite(double value) noexcept { return std::isfinite(value) ? value : 0.0; }
inline double wrap(double phase) noexcept { return std::remainder(finite(phase), tau); }

// Owns no memory: the server supplies a constructor-time RT allocation.
struct MatrixDSP {
    std::size_t n;
    double* phase;
    double* previous;
    double* output;

    MatrixDSP(std::size_t count, double* storage) noexcept
        : n(count), phase(storage), previous(storage + count), output(storage + 2 * count) {}

    template <typename Read> void initialize(Read read) noexcept
    {
        for (std::size_t i = 0; i < n; ++i)
            phase[i] = wrap(read(1 + n + i));
        std::fill(previous, previous + 2 * n, 0.0);
    }

    template <Law law = Law::PM, typename Read> void tick(Read read, double sampleRate,
                                                       unsigned iterations = 4, double damping = 1) noexcept
    {
        if constexpr (law == Law::ZDF) {
            damping = std::clamp(finite(damping), 1e-6, 1.0);
            for (unsigned iteration = 0; iteration < iterations; ++iteration) {
                for (std::size_t i = 0; i < n; ++i) {
                    double sum = 0;
                    for (std::size_t j = 0; j < n; ++j)
                        sum += finite(read(1 + 3 * n + i * n + j)) * previous[j];
                    const double candidate = finite(read(1 + 2*n + i)) * std::sin(wrap(phase[i] + sum));
                    output[i] = damping == 1 ? candidate : previous[i] + damping * (candidate - previous[i]);
                }
                // Jacobi (simultaneous) fixed-point iterations: no traversal bias.
                std::copy(output, output + n, previous);
            }
            for (std::size_t i = 0; i < n; ++i)
                phase[i] = wrap(phase[i] + tau * finite(read(1 + i)) / sampleRate);
            return;
        }
        for (std::size_t i = 0; i < n; ++i) {
            double modulation = 0.0;
            for (std::size_t j = 0; j < n; ++j)
                modulation += finite(read(1 + 3 * n + i * n + j)) * previous[j];
            const double frequency = finite(read(1 + i));
            if constexpr (law == Law::PM) {
                output[i] = finite(read(1 + 2 * n + i)) * std::sin(wrap(phase[i] + modulation));
                phase[i] = wrap(phase[i] + tau * frequency / sampleRate);
            } else if constexpr (law == Law::ExpFM) {
                output[i] = finite(read(1 + 2*n + i)) * std::sin(phase[i]);
                // 128 octaves is a numerical bound, far outside ordinary
                // musical modulation; float input * exp2(128) fits in double.
                const double effective = frequency * std::exp2(std::clamp(modulation, -128.0, 128.0));
                phase[i] = wrap(phase[i] + tau * effective / sampleRate);
            } else {
                output[i] = finite(read(1 + 2 * n + i)) * std::sin(phase[i]);
                phase[i] = wrap(phase[i] + tau * (frequency + modulation) / sampleRate);
            }
        }
        // Commit only after every destination has read the same old vector.
        std::copy(output, output + n, previous);
    }

    template <typename Read> void tickGraph(Read read, const std::uint32_t* sources,
        const std::uint32_t* destinations, std::size_t edges, std::size_t depthStart, double sampleRate) noexcept
    {
        std::fill(output, output + n, 0.0);
        for (std::size_t edge = 0; edge < edges; ++edge)
            output[destinations[edge]] += finite(read(depthStart + edge)) * previous[sources[edge]];
        for (std::size_t i = 0; i < n; ++i) {
            output[i] = finite(read(1 + 2*n + i)) * std::sin(wrap(phase[i] + output[i]));
            phase[i] = wrap(phase[i] + tau * finite(read(1+i)) / sampleRate);
        }
        std::copy(output, output+n, previous);
    }

    // Extended dense PM: only internal operators have delayed state. External
    // sources are read at the current internal sample, after input interpolation.
    template <typename Read, typename Evaluate> void tickExtended(Read read, Evaluate evaluate,
        std::size_t depthStart, std::size_t externalCount, std::size_t externalStart, double sampleRate) noexcept
    {
        for (std::size_t i = 0; i < n; ++i) {
            const auto row = depthStart + i * (n + externalCount);
            double sum = 0;
            for (std::size_t j = 0; j < n; ++j) sum += finite(read(row+j)) * previous[j];
            for (std::size_t k = 0; k < externalCount; ++k)
                sum += finite(read(row+n+k)) * finite(read(externalStart+k));
            output[i] = finite(read(1+2*n+i)) * finite(evaluate(i, phase[i], sum));
            phase[i] = wrap(phase[i] + tau * finite(read(1+i)) / sampleRate);
        }
        std::copy(output, output+n, previous);
    }
};

} // namespace fmmatrix
