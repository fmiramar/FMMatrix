// SPDX-License-Identifier: GPL-3.0-or-later
#include "MatrixLayout.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

void testRequiredLayouts()
{
    for (const std::uint64_t operators : {1U, 2U, 6U, 16U, 31U, 64U}) {
        std::uint64_t inputs = 0U;
        require(fmmatrix::inputCountForOperators(operators, inputs),
            "required operator count must have a representable layout");
        require(inputs == 3U + 5U * operators + operators * operators,
            "layout includes dense matrix, five vectors and three scalar controls");
        require(fmmatrix::isValidLayout(operators, inputs, operators),
            "required operator count must validate");
        require(!fmmatrix::isValidLayout(operators, inputs - 1U, operators),
            "missing input must fail layout validation");
        require(!fmmatrix::isValidLayout(operators, inputs, operators + 1U),
            "wrong output count must fail layout validation");
    }
}

void testInvalidLayouts()
{
    std::uint64_t ignored = 0U;
    require(!fmmatrix::inputCountForOperators(0U, ignored),
        "zero operators must be rejected");
    require(!fmmatrix::inputCountForOperators(std::numeric_limits<std::uint64_t>::max(), ignored),
        "overflowing operator count must be rejected");
}

} // namespace

int main()
{
    testRequiredLayouts();
    testInvalidLayouts();
    std::cout << "FMMatrixUGens layout tests passed\n";
    return 0;
}
