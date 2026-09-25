// SPDX-License-Identifier: GPL-3.0-or-later
#include "MatrixEngine.hpp"
#include <chrono>
#include <complex>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using Complex = std::complex<double>;
void fft(std::vector<Complex>& values)
{
    const auto n = values.size();
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(values[i], values[j]);
    }
    for (std::size_t length = 2; length <= n; length <<= 1) {
        const auto step = std::polar(1.0, -fmmatrix::tau / static_cast<double>(length));
        for (std::size_t start = 0; start < n; start += length) {
            Complex phase(1, 0);
            for (std::size_t i = 0; i < length / 2; ++i) {
                const auto a = values[start + i], b = values[start + i + length / 2] * phase;
                values[start + i] = a + b; values[start + i + length / 2] = a - b; phase *= step;
            }
        }
    }
}

template <fmmatrix::Law law> bool aliasBenchmark(const std::filesystem::path& directory, const char* name)
{
    constexpr std::size_t frames = 16384;
    constexpr int carrierBin = 3500, modulatorBin = 2303;
    const double sourceFrequency = modulatorBin * 48000.0 / frames;
    std::vector<bool> legitimate(frames, false);
    for (int sideband = -32; sideband <= 32; ++sideband) {
        const int bin = carrierBin + sideband * modulatorBin;
        if (std::abs(bin) < static_cast<int>(frames / 2)) {
            legitimate[static_cast<std::size_t>((bin + static_cast<int>(frames)) % static_cast<int>(frames))] = true;
            legitimate[static_cast<std::size_t>((-bin + static_cast<int>(frames)) % static_cast<int>(frames))] = true;
        }
    }
    std::ofstream csv(directory / (std::string("alias_") + name + ".csv"));
    csv << "oversample,alias_energy_db,total_power\n" << std::setprecision(10);
    double base = 0; bool ok = true;
    for (const unsigned factor : {1U, 2U, 4U, 8U}) {
        std::vector<double> inputs{2, carrierBin * 48000.0 / frames, sourceFrequency,
            0, 0, 1, 1, 0, law == fmmatrix::Law::PM ? 4.0 : 4.0 * sourceFrequency, 0, 0};
        std::vector<double> memory(fmmatrix::MatrixEngine::memorySize(2, factor));
        fmmatrix::MatrixEngine engine(2, factor, memory.data());
        engine.initialize([&](std::size_t i) { return inputs[i]; });
        auto read = [&](std::size_t i, unsigned, unsigned) { return inputs[i]; };
        for (std::size_t frame = 0; frame < 4096; ++frame) engine.tick<law>(read, 48000);
        std::vector<Complex> audio(frames);
        for (auto& value : audio) { engine.tick<law>(read, 48000); value = engine.output[0]; }
        fft(audio);
        double total = 0, aliased = 0;
        for (std::size_t bin = 0; bin < frames; ++bin) {
            const double energy = std::norm(audio[bin]);
            total += energy;
            if (!legitimate[bin]) aliased += energy;
        }
        const double db = 10 * std::log10(std::max(aliased / total, 1e-30));
        if (factor == 1) base = db;
        else ok = ok && db < base - 10;
        csv << factor << ',' << db << ',' << total / (frames * frames) << '\n';
        std::cout << name << ' ' << factor << "x: alias energy " << db << " dB\n";
    }
    return ok && csv.good();
}

template <fmmatrix::Law law> void cpuBenchmark(std::ofstream& csv, const char* name)
{
    for (const std::size_t n : {1U, 2U, 4U, 8U, 16U, 32U, 64U}) {
      for (const unsigned factor : {1U, 2U, 4U, 8U}) {
        std::vector<double> inputs(1 + 3*n + n*n, 0), memory(fmmatrix::MatrixEngine::memorySize(n, factor));
        for (std::size_t i = 0; i < n; ++i) {
            inputs[1 + i] = 110 + 7 * static_cast<double>(i); inputs[1 + 2*n + i] = 0.7;
            for (std::size_t j = 0; j < n; ++j) inputs[1 + 3*n + i*n + j] = (law == fmmatrix::Law::FM ? 100 : 0.3) / static_cast<double>(n);
        }
        fmmatrix::MatrixEngine engine(n, factor, memory.data());
        engine.initialize([&](std::size_t i) { return inputs[i]; });
        auto read = [&](std::size_t i, unsigned, unsigned) { return inputs[i]; };
        const std::size_t frames = std::clamp<std::size_t>(2000000 / (n*n*factor), 512, 8192);
        for (int warm = 0; warm < 128; ++warm) engine.tick<law>(read, 48000);
        double timings[3]{}; volatile double checksum = 0;
        for (double& ns : timings) {
            const auto start = std::chrono::steady_clock::now();
            for (std::size_t frame = 0; frame < frames; ++frame) { engine.tick<law>(read, 48000); checksum += engine.output[0]; }
            ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count() / static_cast<double>(frames);
        }
        std::sort(timings, timings + 3);
        csv << name << ',' << n << ',' << factor << ',' << timings[1] << ','
            << timings[1] * 48000 / 1e9 << ',' << memory.size() * sizeof(double) + sizeof(engine) << '\n';
        (void)checksum;
      }
    }
}

void solverBenchmark(const std::filesystem::path& directory)
{
    std::ofstream csv(directory / "solver.csv");
    csv << "operators,matrix_infinity_norm,iterations,damping,residual_rms,residual_max,ns_per_host_sample,finite\n" << std::setprecision(10);
    for (const std::size_t n : {1U, 2U, 4U, 8U}) {
      for (const double strength : {0.2, 0.8, 1.2, 2.5}) {
       for (const unsigned iterations : {1U, 2U, 4U, 8U, 16U, 32U, 64U}) {
        for (const double damping : {0.25, 0.5, 1.0}) {
            std::vector<double> inputs(1 + 3*n + n*n), phase(n), memory(fmmatrix::MatrixEngine::memorySize(n, 1));
            for (std::size_t i = 0; i < n; ++i) {
                inputs[1+i] = 110 + 71 * static_cast<double>(i);
                inputs[1+n+i] = 0.3;
                inputs[1+2*n+i] = 1;
                for (std::size_t j = 0; j < n; ++j)
                    inputs[1+3*n+i*n+j] = ((i+j) % 2 == 0 ? 1.0 : -1.0) * strength / static_cast<double>(n);
            }
            fmmatrix::MatrixEngine engine(n, 1, memory.data());
            engine.initialize([&](std::size_t i) { return inputs[i]; });
            double residualSum = 0, maxResidual = 0;
            bool finite = true;
            const auto start = std::chrono::steady_clock::now();
            for (int frame = 0; frame < 1024; ++frame) {
                std::copy(engine.dsp.phase, engine.dsp.phase+n, phase.begin());
                engine.tick<fmmatrix::Law::ZDF>([&](std::size_t i, unsigned, unsigned) { return inputs[i]; }, 48000, iterations, damping);
                for (std::size_t i = 0; i < n; ++i) {
                    double sum = phase[i];
                    for (std::size_t j = 0; j < n; ++j) sum += inputs[1+3*n+i*n+j] * engine.output[j];
                    const double residual = engine.output[i] - std::sin(sum);
                    finite = finite && std::isfinite(residual);
                    if (frame >= 512) { residualSum += residual*residual; maxResidual = std::max(maxResidual, std::abs(residual)); }
                }
            }
            const double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count() / 1024;
            csv << n << ',' << strength << ',' << iterations << ',' << damping << ','
                << std::sqrt(residualSum / (512 * static_cast<double>(n))) << ',' << maxResidual << ',' << ns << ',' << finite << '\n';
        }
       }
      }
    }
}

void graphBenchmark(const std::filesystem::path& directory)
{
    std::ofstream csv(directory / "sparse.csv");
    csv << "operators,edges,dense_ns_per_sample,sparse_ns_per_sample,speedup\n" << std::setprecision(10);
    for (const std::size_t n : {32U, 64U, 128U, 256U}) {
        const std::size_t prefix = 1+3*n;
        std::vector<double> dense(prefix+n*n), sparse(prefix+n);
        std::vector<std::uint32_t> sources(n), destinations(n);
        for (std::size_t i = 0; i < n; ++i) {
            dense[1+i] = 110 + static_cast<double>(i); dense[1+n+i] = 0.2; dense[1+2*n+i] = 0.7;
            sources[i] = static_cast<std::uint32_t>(i); destinations[i] = static_cast<std::uint32_t>((i+1)%n);
            dense[prefix+destinations[i]*n+sources[i]] = 0.3;
            sparse[prefix+i] = 0.3;
        }
        std::copy(dense.begin(), dense.begin()+static_cast<std::ptrdiff_t>(prefix), sparse.begin());
        double timings[2]{};
        for (int type = 0; type < 2; ++type) {
            std::vector<double> memory(fmmatrix::MatrixEngine::memorySize(n, 1));
            fmmatrix::MatrixEngine engine(n, 1, memory.data());
            engine.initialize([&](std::size_t i) { return dense[i]; });
            volatile double checksum = 0;
            const auto start = std::chrono::steady_clock::now();
            for (int frame = 0; frame < 4096; ++frame) {
                if (type == 0) engine.tick<fmmatrix::Law::PM>([&](std::size_t i, unsigned, unsigned) { return dense[i]; }, 48000);
                else engine.tickGraph([&](std::size_t i, unsigned, unsigned) { return sparse[i]; }, sources.data(), destinations.data(), n, prefix, 48000);
                checksum += engine.output[0];
            }
            timings[type] = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now()-start).count()/4096;
            (void)checksum;
        }
        csv << n << ',' << n << ',' << timings[0] << ',' << timings[1] << ',' << timings[0]/timings[1] << '\n';
    }
}

int main(int argc, char** argv)
{
    if (argc != 2) { std::cerr << "Usage: fm_matrix_benchmark output-directory\n"; return 1; }
    const std::filesystem::path directory(argv[1]);
    std::filesystem::create_directories(directory);
    const bool pmOK = aliasBenchmark<fmmatrix::Law::PM>(directory, "PMMatrix");
    const bool fmOK = aliasBenchmark<fmmatrix::Law::FM>(directory, "FMMatrix");
    std::ofstream csv(directory / "oversampling.csv");
    csv << "ugen,operators,oversample,ns_per_host_sample,realtime_fraction_48k,dsp_state_bytes\n" << std::setprecision(10);
    cpuBenchmark<fmmatrix::Law::PM>(csv, "PMMatrix");
    cpuBenchmark<fmmatrix::Law::FM>(csv, "FMMatrix");
    cpuBenchmark<fmmatrix::Law::ZDF>(csv, "PMMatrixZDF");
    solverBenchmark(directory);
    graphBenchmark(directory);
    if (!(pmOK && fmOK && csv.good())) { std::cerr << "Alias reduction benchmark failed\n"; return 1; }
}
