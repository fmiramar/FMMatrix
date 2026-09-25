// SPDX-License-Identifier: GPL-3.0-or-later
#include "Decimator.hpp"
#include <complex>
#include <cstdlib>
#include <iostream>
#include <vector>

void require(bool ok, const char* message)
{
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

int main()
{
    std::vector<double> memory(fmmatrix::Decimator::memorySize(1, 2));
    fmmatrix::Decimator filter(1, 2, memory.data());
    double passError = 0, stopMax = 0;
    for (int bin = 0; bin <= 1000; ++bin) {
        const double frequency = static_cast<double>(bin) / 2000;
        std::complex<double> response{};
        for (std::size_t tap = 0; tap < filter.taps; ++tap)
            response += filter.kernel[tap] * std::polar(1.0, -fmmatrix::tau * frequency * static_cast<double>(tap));
        if (frequency <= 0.18) passError = std::max(passError, std::abs(std::abs(response) - 1));
        if (frequency >= 0.32) stopMax = std::max(stopMax, std::abs(response));
    }
    std::cout << "Half-band pass error " << passError << ", stopband amplitude " << stopMax << '\n';
    require(passError < 0.001, "half-band passband error");
    require(stopMax < 0.0001, "half-band stopband at least 80 dB");
    for (const unsigned factor : {2U, 4U, 8U}) {
      std::vector<double> cascadeMemory(fmmatrix::Decimator::memorySize(1, factor));
      fmmatrix::Decimator cascade(1, factor, cascadeMemory.data());
      for (const double hostFrequency : {0.0, 0.1, 0.8}) {
        const double frequency = hostFrequency / factor;
        cascade.reset(); double sum = 0; int count = 0;
        for (int frame = 0; frame < 32000; ++frame) {
            double value = frequency == 0 ? 1 : std::sin(fmmatrix::tau * frequency * frame);
            const bool emitted = cascade.push(&value);
            require(emitted == (static_cast<unsigned>(frame) % factor == 0), "cascade decimator output alignment");
            if (emitted && frame > 1024) { sum += value * value; ++count; }
        }
        const double rms = std::sqrt(sum / count);
        if (frequency == 0) require(std::abs(rms - 1) < 1e-12, "DC unity gain");
        else if (hostFrequency == 0.1) require(std::abs(rms - std::sqrt(0.5)) < 0.0003, "passband tone retained");
        else require(rms < 0.0001, "out-of-band tone rejected before decimation");
    }
    }
    std::cout << "FMMatrixUGens decimator tests passed\n";
}
