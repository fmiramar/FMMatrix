// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "MatrixDSP.hpp"

namespace fmmatrix {

// Raw cyclic mono samples, not SuperCollider's interleaved Osc wavetable format.
inline double cycleTable(const float* data, std::size_t frames, double phase) noexcept
{
    if (!data || frames < 2) return 0;
    const double cycles = wrap(phase) / tau;
    const double position = (cycles - std::floor(cycles)) * static_cast<double>(frames);
    // Rounding immediately below cycle 1 can produce exactly `frames`.
    const auto index = static_cast<std::size_t>(position);
    const auto first = index < frames ? index : 0;
    const auto second = first + 1 == frames ? 0 : first + 1;
    const double fraction = position - static_cast<double>(index);
    return finite(data[first]) * (1-fraction) + finite(data[second]) * fraction;
}

inline double nonlinear(double x, unsigned shape) noexcept
{
    x = finite(x);
    switch (shape) {
    case 1: return std::tanh(x);
    case 2: return x / (1 + std::abs(x));
    case 3: { // triangle fold into [-1,1], period 4
        const double t = std::fmod(std::fmod(x + 1, 4) + 4, 4);
        return t <= 2 ? t - 1 : 3 - t;
    }
    case 4: return std::fmod(std::fmod(x + 1, 2) + 2, 2) - 1;
    case 5: return std::abs(x);
    case 6: return finite(x*x);
    case 7: return x > 0 ? 1 : x < 0 ? -1 : 0;
    default: return x;
    }
}

// One shared ring per SOURCE; every edge can select a separate fractional delay.
// The next-write cursor advances only after all destinations have been computed.
struct DelayHistory {
    double* data;
    std::size_t n, length, position = 0;
    double maximum;
    DelayHistory(double* storage, std::size_t count, std::size_t frames, double maxSamples) noexcept
        : data(storage), n(count), length(frames), maximum(maxSamples) {}
    void reset() noexcept { std::fill(data, data + n*length, 0); position = 0; }
    double read(std::size_t source, double samples, unsigned minimum) const noexcept
    {
        samples = std::clamp(finite(samples), static_cast<double>(minimum), maximum);
        const auto whole = static_cast<std::size_t>(samples);
        const auto first = (position + length - whole) % length;
        const auto second = first == 0 ? length - 1 : first - 1;
        const double fraction = samples - static_cast<double>(whole);
        return data[source*length+first]*(1-fraction) + data[source*length+second]*fraction;
    }
    void push(const double* values) noexcept
    {
        for (std::size_t i = 0; i < n; ++i) data[i*length+position] = values[i];
        if (++position == length) position = 0;
    }
};
} // namespace fmmatrix
