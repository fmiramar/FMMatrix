// SPDX-License-Identifier: GPL-3.0-or-later
#include "MatrixEngine.hpp"
#include "MatrixLayout.hpp"
#include <cstdlib>
#include <iostream>
#include <vector>

void require(bool ok, const char* message)
{
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
int main()
{
    std::uint64_t count = 0;
    require(fmmatrix::graphInputCount(256, 512, count) && count == 2820, "sparse input layout scales with N+E");
    require(!fmmatrix::graphInputCount(0, 1, count), "empty graph operator list rejected");
    require(!fmmatrix::graphInputCount(1, UINT64_MAX, count), "sparse layout overflow rejected");
    for (const std::size_t n : {1U, 2U, 6U, 31U, 64U}) {
      for (const unsigned factor : {1U, 2U, 4U, 8U}) {
        const auto prefix = 1 + 3*n;
        std::vector<double> dense(prefix+n*n), sparse(prefix+2*n);
        std::vector<std::uint32_t> sources(2*n), destinations(2*n);
        for (std::size_t i = 0; i < n; ++i) {
            dense[1+i] = (i % 2 == 0 ? 1 : -1) * (110 + 13*static_cast<double>(i));
            dense[1+n+i] = 0.3; dense[1+2*n+i] = 0.7;
            sources[2*i] = static_cast<std::uint32_t>((i+1)%n);
            sources[2*i+1] = static_cast<std::uint32_t>(i);
            destinations[2*i] = destinations[2*i+1] = static_cast<std::uint32_t>(i);
        }
        std::copy(dense.begin(), dense.begin()+static_cast<std::ptrdiff_t>(prefix), sparse.begin());
        std::vector<double> aMemory(fmmatrix::MatrixEngine::memorySize(n, factor)), bMemory(aMemory.size());
        fmmatrix::MatrixEngine a(n, factor, aMemory.data()), b(n, factor, bMemory.data());
        a.initialize([&](std::size_t i) { return dense[i]; });
        b.initialize([&](std::size_t i) { return sparse[i]; });
        for (int frame = 0; frame < 512; ++frame) {
            std::fill(dense.begin()+static_cast<std::ptrdiff_t>(prefix), dense.end(), 0);
            for (std::size_t edge = 0; edge < 2*n; ++edge) {
                const double depth = 0.1 + 0.2*std::sin(0.003*frame + static_cast<double>(edge));
                sparse[prefix+edge] = depth;
                dense[prefix + destinations[edge]*n + sources[edge]] += depth;
            }
            a.tick<fmmatrix::Law::PM>([&](std::size_t i, unsigned, unsigned) { return dense[i]; }, 48000);
            b.tickGraph([&](std::size_t i, unsigned, unsigned) { return sparse[i]; }, sources.data(), destinations.data(), 2*n, prefix, 48000);
            for (std::size_t i = 0; i < n; ++i)
                require(std::abs(a.output[i] - b.output[i]) < 1e-12, "sparse/dense dynamic cyclic, self and duplicate-edge equivalence");
        }
      }
    }
    for (const std::size_t n : {32U, 64U, 128U, 256U}) {
        std::vector<double> inputs(1+3*n), memory(fmmatrix::MatrixEngine::memorySize(n, 1));
        for (std::size_t i = 0; i < n; ++i) { inputs[1+i] = -100; inputs[1+n+i] = 0.2; inputs[1+2*n+i] = 0.7; }
        fmmatrix::MatrixEngine engine(n, 1, memory.data());
        engine.initialize([&](std::size_t i) { return inputs[i]; });
        for (int frame = 0; frame < 1024; ++frame) {
            engine.tickGraph([&](std::size_t i, unsigned, unsigned) { return inputs[i]; }, nullptr, nullptr, 0, 1+3*n, 48000);
            for (std::size_t i = 0; i < n; ++i)
                require(std::abs(engine.output[i] - 0.7*std::sin(0.2 - fmmatrix::tau*100*frame/48000)) < 1e-12, "large zero-edge graph / signed-frequency reference");
        }
    }
    std::cout << "FMMatrixUGens graph tests passed\n";
}
