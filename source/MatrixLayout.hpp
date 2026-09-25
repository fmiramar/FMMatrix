// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <limits>

namespace fmmatrix {

// N, freqs[N], phases[N], amps[N], matrix[N*N], resetTrig[N],
// resetPhase[N], oversample, smooth. The original operator prefix is stable.
inline bool inputCountForOperators(std::uint64_t operators,
                                   std::uint64_t& result) noexcept
{
    if (operators == 0U)
        return false;
    constexpr std::uint64_t kMax = std::numeric_limits<std::uint64_t>::max();
    if (operators > kMax / operators)
        return false;
    const std::uint64_t matrix = operators * operators;
    if (matrix > kMax - 3U || operators > (kMax - matrix - 3U) / 5U)
        return false;
    result = 3U + 5U * operators + matrix;
    return true;
}

inline bool isValidLayout(std::uint64_t operators, std::uint64_t inputs,
                          std::uint64_t outputs, std::uint64_t extraInputs = 0) noexcept
{
    std::uint64_t required = 0U;
    return inputCountForOperators(operators, required)
        && extraInputs <= std::numeric_limits<std::uint64_t>::max() - required
        && inputs == required + extraInputs && outputs == operators;
}

inline bool graphInputCount(std::uint64_t operators, std::uint64_t edges, std::uint64_t& result) noexcept
{
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    if (operators == 0 || operators > (maximum - 4) / 5) return false;
    const auto prefix = 4 + 5 * operators;
    if (edges > (maximum - prefix) / 3) return false;
    result = prefix + 3 * edges;
    return true;
}

inline bool externalInputCount(std::uint64_t n, std::uint64_t k, std::uint64_t& result) noexcept
{
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    if (n == 0 || k > maximum-4 || n > (maximum-4-k)/5 || k > maximum-n) return false;
    const auto prefix = 4+5*n+k;
    if (n > (maximum-prefix)/(n+k)) return false;
    result = prefix+n*(n+k);
    return true;
}

} // namespace fmmatrix
