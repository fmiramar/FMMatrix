// SPDX-License-Identifier: GPL-3.0-or-later
#include "ExtendedDSP.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
#if defined(__SSE__) || defined(_M_X64)
#include <xmmintrin.h>
#endif

double polynomialSine(double x)
{
    if (x > fmmatrix::tau/4) x = fmmatrix::tau/2-x;
    else if (x < -fmmatrix::tau/4) x = -fmmatrix::tau/2-x;
    const double square = x*x;
    return x*(1 + square*(-1.0/6 + square*(1.0/120 - square/5040)));
}

template <typename Evaluate> void sineRow(std::ofstream& csv, const char* name, Evaluate evaluate)
{
    constexpr std::size_t count = 65536;
    std::vector<double> phases(count), reference(count), values(count);
    for (std::size_t i = 0; i < count; ++i) {
        phases[i] = fmmatrix::wrap(fmmatrix::tau*137*static_cast<double>(i)/count);
        reference[i] = static_cast<double>(std::sin(static_cast<long double>(phases[i])));
    }
    double peak = 0, energy = 0, fundamentalSin = 0, fundamentalCos = 0;
    for (std::size_t i = 0; i < count; ++i) {
        values[i] = evaluate(phases[i]);
        const double error = values[i]-reference[i];
        peak = std::max(peak, std::abs(error)); energy += error*error;
        fundamentalSin += values[i]*std::sin(phases[i]); fundamentalCos += values[i]*std::cos(phases[i]);
    }
    fundamentalSin *= 2.0/count; fundamentalCos *= 2.0/count;
    double distortion = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const double residual = values[i]-fundamentalSin*std::sin(phases[i])-fundamentalCos*std::cos(phases[i]);
        distortion += residual*residual;
    }
    double times[3]{}; volatile double checksum = 0;
    for (double& ns : times) {
        const auto start = std::chrono::steady_clock::now();
        for (int repeat = 0; repeat < 8; ++repeat) for (double phase : phases) checksum += evaluate(phase);
        ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now()-start).count()/(count*8);
    }
    std::sort(times, times+3);
    csv << name << ',' << peak << ',' << std::sqrt(energy/count) << ','
        << 10*std::log10(std::max(distortion/count/0.5, 1e-30)) << ',' << times[1] << '\n';
    (void)checksum;
}

void denormalRows(std::ofstream& csv)
{
    csv << "fp_mode,seed,ns_per_sample,final_output\n" << std::setprecision(12);
#if defined(__SSE__) || defined(_M_X64)
    const unsigned saved = _mm_getcsr();
    const unsigned modes[]{0, 0x8000, 0x8040};
    const char* names[]{"ieee", "flush_to_zero", "flush_and_denormals_are_zero"};
    for (std::size_t mode = 0; mode < 3; ++mode) {
        _mm_setcsr((saved & ~0x8040U) | modes[mode]);
        const double seeds[]{0.0, 1e-100, 1e-310};
        const char* seedNames[]{"0", "1e-100", "1e-310"};
        for (std::size_t seedIndex = 0; seedIndex < 3; ++seedIndex) {
            const double seed = seeds[seedIndex];
            double memory[3]{}, inputs[]{1, 0, 0, 1, 1};
            fmmatrix::MatrixDSP dsp(1, memory);
            auto read = [&](std::size_t i) { return inputs[i]; };
            double times[3]{}; volatile double checksum = 0;
            for (double& ns : times) {
                dsp.initialize(read); dsp.previous[0] = seed;
                const auto start = std::chrono::steady_clock::now();
                for (int frame = 0; frame < 100000; ++frame) { dsp.tick(read, 48000); checksum += dsp.output[0]; }
                ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now()-start).count()/100000;
            }
            std::sort(times, times+3);
            csv << names[mode] << ',' << seedNames[seedIndex] << ',' << times[1] << ',' << dsp.output[0] << '\n';
            (void)checksum;
        }
    }
    _mm_setcsr(saved);
#else
    // The x86 mode-switch experiment is intentionally omitted on other CPUs.
    (void)csv;
#endif
}

int main(int argc, char** argv)
{
    if (argc != 2) { std::cerr << "Usage: fm_matrix_numerics_benchmark output-directory\n"; return 1; }
    const std::filesystem::path directory(argv[1]); std::filesystem::create_directories(directory);
    std::ofstream sums(directory/"summation.csv"), sine(directory/"sine.csv"), denormals(directory/"denormals.csv");
    sums << "operators,reference,float_sum,double_sum,float_abs_error,double_abs_error\n" << std::setprecision(12);
    for (std::size_t n : {4U, 8U, 16U, 32U, 64U, 128U, 256U}) {
        float single = 0; double twice = 0; long double reference = 0;
        // Float-representable inputs with cancellation; a single destination row.
        const float pattern[]{100000000.0F, 0.25F, -100000000.0F, 0.25F};
        for (std::size_t i = 0; i < n; ++i) { const float value = pattern[i%4]; single += value; twice += value; reference += value; }
        sums << n << ',' << reference << ',' << single << ',' << twice << ','
            << std::abs(static_cast<long double>(single)-reference) << ',' << std::abs(static_cast<long double>(twice)-reference) << '\n';
    }
    std::vector<float> table(8192);
    for (std::size_t i = 0; i < table.size(); ++i) table[i] = static_cast<float>(std::sin(fmmatrix::tau*static_cast<double>(i)/table.size()));
    sine << "method,max_abs_error,rms_error,nonfundamental_energy_db,ns_per_evaluation\n" << std::setprecision(12);
    sineRow(sine, "std_sin", [](double phase) { return std::sin(phase); });
    sineRow(sine, "linear_float_table_8192", [&](double phase) { return fmmatrix::cycleTable(table.data(), table.size(), phase); });
    sineRow(sine, "folded_taylor_7", polynomialSine);
    denormalRows(denormals);
    const bool ok = sums.good() && sine.good() && denormals.good();
    std::cout << (ok ? "Numerics benchmark complete\n" : "Numerics benchmark failed\n");
    return ok ? 0 : 1;
}
