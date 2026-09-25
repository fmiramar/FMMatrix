// SPDX-License-Identifier: GPL-3.0-or-later
// Real server callback, mock host allocation: includes rate conditioning but
// excludes server scheduling, hardware I/O and upstream modulation generation.
#include "FMMatrixUGens.cpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

namespace {
void* allocate(World*, std::size_t bytes) { return std::malloc(bytes); }
void release(World*, void* pointer) { std::free(pointer); }
int quiet(const char*, ...) { return 0; }

template <fmmatrix::Law law> bool measure(std::ofstream& csv, const char* name)
{
    constexpr int blockSize = 64;
    for (std::uint32_t n : {1U, 2U, 4U, 8U, 16U, 32U, 64U})
    for (unsigned factor : {1U, 2U, 4U, 8U})
    for (int inputRate : {calc_ScalarRate, calc_BufRate, calc_FullRate})
    for (unsigned iterations : {1U, 2U, 4U, 8U}) {
        if constexpr (law != fmmatrix::Law::ZDF) { if (iterations != 1) continue; }
        std::uint64_t base = 0;
        fmmatrix::inputCountForOperators(n, base);
        const auto count = static_cast<std::size_t>(base) + (law == fmmatrix::Law::ZDF ? 2 : 0);
        PMMatrix unit{}; World world{}; Rate rate{};
        rate.mSampleRate = 48000; rate.mSampleDur = 1.0/48000;
        unit.mWorld = &world; unit.mRate = &rate; unit.mCalcRate = calc_FullRate; unit.mBufLength = blockSize;
        unit.mNumInputs = static_cast<std::uint32_t>(count); unit.mNumOutputs = n;
        std::vector<Wire> wires(count);
        std::vector<Wire*> wirePointers(count);
        std::vector<float> input(count*blockSize, 0), output(n*blockSize, 0);
        std::vector<float*> in(count), out(n);
        for (std::size_t i = 0; i < count; ++i) {
            wirePointers[i] = &wires[i]; wires[i].mCalcRate = calc_ScalarRate; in[i] = input.data()+i*blockSize;
        }
        for (std::size_t i = 0; i < n; ++i) {
            out[i] = output.data()+i*blockSize;
            in[1+i][0] = 110+7*static_cast<float>(i); in[1+n+i][0] = 0.2F; in[1+2*n+i][0] = 0.7F;
        }
        in[0][0] = static_cast<float>(n); in[base-2][0] = static_cast<float>(factor);
        in[base-1][0] = 0.005F;
        if constexpr (law == fmmatrix::Law::ZDF) { in[count-2][0] = static_cast<float>(iterations); in[count-1][0] = 1; }
        const std::size_t begin = 1+3*n, end = begin+static_cast<std::size_t>(n)*n;
        const float strength = (law == fmmatrix::Law::FM ? 100.0F : 0.3F)/static_cast<float>(n);
        for (std::size_t i = begin; i < end; ++i) {
            wires[i].mCalcRate = inputRate;
            for (int frame = 0; frame < blockSize; ++frame)
                in[i][frame] = strength * (1 + 0.1F*std::sin(static_cast<float>(frame)*0.03F));
        }
        unit.mInput = wirePointers.data(); unit.mInBuf = in.data(); unit.mOutBuf = out.data();
        Matrix_Ctor<law>(&unit);
        if (!unit.valid) { PMMatrix_Dtor(&unit); return false; }
        unit.mCalcFunc(&unit, blockSize); unit.mCalcFunc(&unit, blockSize);
        const unsigned blocks = std::clamp(32768U/(n*n*factor*iterations), 4U, 32U);
        double timings[3]{};
        for (double& ns : timings) {
            double elapsed = 0;
            for (unsigned block = 0; block < blocks; ++block) {
                if (inputRate == calc_BufRate)
                    for (std::size_t i = begin; i < end; ++i) in[i][0] = strength * (block % 2 ? 1.1F : 0.9F);
                const auto start = std::chrono::steady_clock::now();
                unit.mCalcFunc(&unit, blockSize);
                elapsed += std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now()-start).count();
            }
            ns = elapsed/(blocks*blockSize);
        }
        std::sort(timings, timings+3);
        const bool finite = std::all_of(output.begin(), output.end(), [](float x) { return std::isfinite(x); });
        const char* rateName = inputRate == calc_ScalarRate ? "constant" : inputRate == calc_BufRate ? "control" : "audio";
        csv << name << ',' << n << ',' << factor << ',' << rateName << ',' << iterations << ',' << timings[1]
            << ',' << timings[1]*48000/1e9 << ',' << finite << '\n';
        PMMatrix_Dtor(&unit);
        if (!finite) return false;
    }
    return csv.good();
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) { std::cerr << "Usage: fm_matrix_adapter_benchmark output-directory\n"; return 1; }
    InterfaceTable table{}; table.fRTAlloc = allocate; table.fRTFree = release; table.fPrint = quiet; ft = &table;
    std::filesystem::create_directories(argv[1]);
    std::ofstream csv(std::filesystem::path(argv[1])/"input_rates.csv");
    csv << "ugen,operators,oversample,matrix_rate,iterations,ns_per_host_sample,realtime_fraction_48k,finite\n" << std::setprecision(10);
    const bool ok = measure<fmmatrix::Law::PM>(csv, "PMMatrix") && measure<fmmatrix::Law::FM>(csv, "FMMatrix")
        && measure<fmmatrix::Law::ZDF>(csv, "PMMatrixZDF");
    std::cout << (ok ? "Adapter rate benchmark complete\n" : "Adapter rate benchmark failed\n");
    return ok ? 0 : 1;
}
