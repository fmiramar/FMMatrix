// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "MatrixDSP.hpp"
#include <array>

namespace fmmatrix {

// Symmetric 63-tap half-band FIR. A four-term Blackman-Harris window is applied to the ideal
// sinc low-pass at 1/4 cycle/input-sample. Center tap is exactly 1/2; the other
// nonzero taps are normalized to sum to 1/2. No runtime coefficient design.
struct Decimator {
    static constexpr std::size_t taps = 63;
    std::size_t n, stages;
    double* memory;
    std::array<double, taps> kernel{};
    std::array<std::size_t, 3> position{};
    std::array<bool, 3> odd{};

    static std::size_t stageCount(unsigned factor) noexcept
    {
        return factor == 8 ? 3 : factor == 4 ? 2 : factor == 2 ? 1 : 0;
    }
    static std::size_t memorySize(std::size_t count, unsigned factor) noexcept
    {
        return count * taps * stageCount(factor);
    }
    Decimator(std::size_t count, unsigned factor, double* storage) noexcept
        : n(count), stages(stageCount(factor)), memory(storage)
    {
        double sum = 0;
        for (std::size_t i = 0; i < taps; i += 2) {
            const double k = static_cast<double>(i) - 31;
            const double angle = tau * static_cast<double>(i) / 62;
            const double window = 0.35875 - 0.48829 * std::cos(angle)
                + 0.14128 * std::cos(2 * angle) - 0.01168 * std::cos(3 * angle);
            kernel[i] = std::sin(tau * 0.25 * k) / (tau * 0.5 * k) * window;
            sum += kernel[i];
        }
        for (std::size_t i = 0; i < taps; i += 2) kernel[i] *= 0.5 / sum;
        kernel[31] = 0.5;
        reset();
    }
    void reset() noexcept
    {
        std::fill(memory, memory + n * taps * stages, 0.0);
        position.fill(0); odd.fill(false);
    }
    // 'values' is scratch, overwritten between stages. The first input and
    // then every Rth input produce output, aligning output with host time n.
    bool push(double* values) noexcept
    {
        for (std::size_t stage = 0; stage < stages; ++stage) {
            double* buffer = memory + stage * taps * n;
            const std::size_t p = position[stage];
            std::copy(values, values + n, buffer + p * n);
            position[stage] = (p + 1) % taps;
            const bool emit = !odd[stage];
            odd[stage] = emit;
            if (!emit) return false;
            for (std::size_t i = 0; i < n; ++i) {
                double sum = 0.5 * buffer[((p + taps - 31) % taps) * n + i];
                for (std::size_t tap = 0; tap < taps; tap += 2)
                    sum += kernel[tap] * buffer[((p + taps - tap) % taps) * n + i];
                values[i] = sum;
            }
        }
        return true;
    }
};

} // namespace fmmatrix
