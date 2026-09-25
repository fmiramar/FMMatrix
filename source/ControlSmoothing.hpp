// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "MatrixDSP.hpp"

namespace fmmatrix {
inline double ramp(double start, double end, double fraction) noexcept
{
    return start + (end - start) * fraction;
}
inline double lagAmount(double seconds, double sampleRate) noexcept
{
    // Time to reach 1 - exp(-1) of the target, not SC Lag's 60 dB time.
    return seconds > 0 && std::isfinite(seconds) ? -std::expm1(-1 / (seconds * sampleRate)) : 1;
}
inline double lag(double old, double target, double amount) noexcept
{
    if (amount >= 1) return target;
    const double result = old + amount * (target - old);
    return std::abs(result) < 1e-300 ? 0 : result;
}
inline void resetPhase(double& phase, double& oldTrigger, double trigger, double target) noexcept
{
    trigger = finite(trigger);
    if (trigger > 0 && oldTrigger <= 0) phase = wrap(target);
    oldTrigger = trigger;
}
} // namespace fmmatrix
