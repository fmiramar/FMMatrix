// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the actual server adapter with guarded input buffers and a mock
// RT allocator. ASan/UBSan can inspect malformed SynthDef and failure paths
// without injecting a sanitizer into the user's audio server.
#include "FMMatrixUGens.cpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

namespace {
bool calculating = false;
int allocations = 0, liveAllocations = 0, failAt = -1;

void require(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void* allocate(World*, std::size_t bytes)
{
    require(!calculating, "allocation in audio callback");
    if (allocations++ == failAt) return nullptr;
    void* memory = std::malloc(bytes);
    if (memory) { ++liveAllocations; std::memset(memory, 0xAD, bytes); }
    return memory;
}
void release(World*, void* memory)
{
    require(!calculating, "free in audio callback");
    if (memory) { --liveAllocations; std::free(memory); }
}
int quiet(const char*, ...) { return 0; }

template <fmmatrix::Law law> void exercise(std::uint32_t n, unsigned factor, int fault)
{
    constexpr int frames = 64;
    constexpr bool graph = law == fmmatrix::Law::Graph || law == fmmatrix::Law::DelayGraph;
    std::uint64_t base = 0;
    if constexpr (graph) fmmatrix::graphInputCount(n, n, base);
    else if constexpr (law == fmmatrix::Law::In) fmmatrix::externalInputCount(n, 1, base);
    else fmmatrix::inputCountForOperators(n, base);
    const std::size_t extra = law == fmmatrix::Law::ZDF ? 2U : (law == fmmatrix::Law::Wave || law == fmmatrix::Law::NL) ? n
        : law == fmmatrix::Law::DelayGraph ? n+1U : 0U;
    std::size_t count = static_cast<std::size_t>(base) + extra;
    if (fault == 1) --count; // Missing scalar at end of a hand-authored layout.
    PMMatrix unit{};
    World world{}; Rate rate{};
    SndBuf buffer{};
    float cycle[]{0, 0.7F, 1, 0.7F, 0, -0.7F, -1, -0.7F};
    buffer.data = cycle; buffer.frames = buffer.samples = 8; buffer.channels = 1;
    world.mSndBufs = &buffer; world.mNumSndBufs = 1;
    rate.mSampleRate = 48000; rate.mSampleDur = 1.0 / 48000;
    unit.mWorld = &world; unit.mRate = &rate; unit.mCalcRate = calc_FullRate; unit.mBufLength = frames;
    unit.mNumInputs = static_cast<std::uint32_t>(count); unit.mNumOutputs = n;
    std::vector<Wire> wires(count);
    std::vector<Wire*> wirePointers(count);
    std::vector<std::vector<float>> inputs(count, std::vector<float>(frames, 0));
    std::vector<std::vector<float>> outputs(n, std::vector<float>(frames, -9));
    std::vector<float*> inputPointers(count), outputPointers(n);
    for (std::size_t i = 0; i < count; ++i) {
        wires[i].mCalcRate = calc_ScalarRate; wirePointers[i] = &wires[i]; inputPointers[i] = inputs[i].data();
    }
    for (std::size_t i = 0; i < n; ++i) {
        outputPointers[i] = outputs[i].data();
        std::fill(inputs[1+i].begin(), inputs[1+i].end(), (i % 2 == 0 ? 1.0F : -1.0F) * (110 + static_cast<float>(i)));
        std::fill(inputs[1+n+i].begin(), inputs[1+n+i].end(), 0.2F);
        std::fill(inputs[1+2*n+i].begin(), inputs[1+2*n+i].end(), 0.7F);
        if (i % 3 == 0) wires[1+i].mCalcRate = calc_FullRate;
    }
    if constexpr (graph) {
        inputs[1+3*n][0] = static_cast<float>(n);
        for (std::size_t i = 0; i < n; ++i) {
            inputs[2+3*n+i][0] = static_cast<float>(i);
            inputs[2+4*n+i][0] = static_cast<float>((i+1)%n);
            std::fill(inputs[2+5*n+i].begin(), inputs[2+5*n+i].end(), 0.02F);
            wires[2+5*n+i].mCalcRate = i % 2 == 0 ? calc_FullRate : calc_BufRate;
        }
        if (fault == 8) inputs[2+3*n][0] = -1;
        if (fault == 9) wires[1+3*n].mCalcRate = calc_BufRate;
    } else {
        const std::size_t begin = law == fmmatrix::Law::In ? 3+3*n : 1+3*n;
        const std::size_t end = begin + n*(n + (law == fmmatrix::Law::In ? 1U : 0U));
        for (std::size_t i = begin; i < end; ++i) {
            std::fill(inputs[i].begin(), inputs[i].end(), 0.02F);
            wires[i].mCalcRate = i % 2 == 0 ? calc_FullRate : calc_BufRate;
        }
        if constexpr (law == fmmatrix::Law::In) {
            inputs[1+3*n][0] = 1;
            std::fill(inputs[2+3*n].begin(), inputs[2+3*n].end(), 0.25F);
            wires[2+3*n].mCalcRate = calc_FullRate;
            if (fault == 8) inputs[1+3*n][0] = -1;
            if (fault == 9) wires[1+3*n].mCalcRate = calc_BufRate;
        }
    }
    inputs[0][0] = static_cast<float>(n);
    if (fault == 2) inputs[0][0] = std::numeric_limits<float>::quiet_NaN();
    if (fault == 3) wires[0].mCalcRate = calc_FullRate;
    if (fault != 1) {
        inputs[base-2][0] = static_cast<float>(factor);
        inputs[base-1][0] = 0.01F;
        if constexpr (law == fmmatrix::Law::ZDF) { inputs[count-2][0] = 4; inputs[count-1][0] = 0.8F; }
        if constexpr (law == fmmatrix::Law::DelayGraph) {
            for (std::size_t i = base; i < count-1; ++i) std::fill(inputs[i].begin(), inputs[i].end(), 12.5F/48000);
            inputs[count-1][0] = fault == 11 ? -1 : 0.003F;
        }
        if constexpr (law == fmmatrix::Law::Wave || law == fmmatrix::Law::NL) {
            if (fault == 8) wires[base].mCalcRate = calc_FullRate;
            if constexpr (law == fmmatrix::Law::NL) if (fault == 9) inputs[base][0] = 8;
        }
    }
    if (fault == 4) inputs[base-2][0] = 3;
    if (fault == 5) inputs[1][0] = std::numeric_limits<float>::infinity();
    allocations = 0; failAt = fault == 6 ? 0 : fault == 7 ? 1 : fault == 10 ? 2 : fault == 12 ? 3 : -1;
    unit.mInput = wirePointers.data(); unit.mInBuf = inputPointers.data(); unit.mOutBuf = outputPointers.data();
    Matrix_Ctor<law>(&unit);
    const bool shouldWork = fault == 0 || fault == 5;
    require(unit.valid == shouldWork, "defensive constructor validation / allocation failure");
    calculating = true;
    for (int block = 0; block < 4; ++block) unit.mCalcFunc(&unit, frames);
    calculating = false;
    double peak = 0;
    for (const auto& channel : outputs) for (float value : channel) {
        require(std::isfinite(value), "server output must remain finite"); peak = std::max(peak, std::abs(static_cast<double>(value)));
    }
    require(shouldWork ? peak > 0.001 : peak == 0, "valid audio / malformed-layout silence");
    PMMatrix_Dtor(&unit);
    require(liveAllocations == 0, "constructor/destructor leaked RT memory");
}

void bufferSafety()
{
    PMMatrix unit{}; World world{}; Graph parent{}; SndBuf global{}, local{};
    float samples[]{1, 2, 3, 4};
    global.data = local.data = samples; global.frames = local.frames = 4; global.channels = local.channels = 1;
    unit.mWorld = &world; unit.mParent = &parent;
    world.mSndBufs = &global; world.mNumSndBufs = 1;
    parent.mLocalSndBufs = &local; parent.localBufNum = 0;
    require(bufferWave(&unit, 0, 0) == 1 && bufferWave(&unit, 1, 0) == 1, "global and local buffer lookup");
    for (double index : {-1.0, 0.5, 2.0, 1e30, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
        require(bufferWave(&unit, index, 0) == 0, "invalid table must not fall back to buffer zero");
    global.channels = 2;
    require(bufferWave(&unit, 0, 0) == 0, "stereo table is silent");
    global.channels = 1; global.data = nullptr;
    require(bufferWave(&unit, 0, 0) == 0, "freed/unallocated table is silent");
    parent.localBufNum = -1;
    require(bufferWave(&unit, 1, 0) == 0, "unallocated local table is silent");
}
} // namespace

int main()
{
    InterfaceTable table{};
    table.fRTAlloc = allocate; table.fRTFree = release; table.fPrint = quiet; ft = &table;
    bufferSafety();
    for (const std::uint32_t n : {1U, 2U, 3U, 6U, 8U, 16U, 31U, 64U}) {
        for (const unsigned factor : {1U, 2U, 4U, 8U}) {
            exercise<fmmatrix::Law::PM>(n, factor, 0);
            exercise<fmmatrix::Law::FM>(n, factor, 0);
            exercise<fmmatrix::Law::ZDF>(n, factor, 0);
            exercise<fmmatrix::Law::Graph>(n, factor, 0);
            exercise<fmmatrix::Law::ExpFM>(n, factor, 0);
            exercise<fmmatrix::Law::Wave>(n, factor, 0);
            exercise<fmmatrix::Law::In>(n, factor, 0);
            exercise<fmmatrix::Law::DelayGraph>(n, factor, 0);
            exercise<fmmatrix::Law::NL>(n, factor, 0);
        }
    }
    for (int fault = 1; fault <= 10; ++fault) exercise<fmmatrix::Law::Graph>(2, 2, fault);
    for (const unsigned n : {128U, 256U}) exercise<fmmatrix::Law::Graph>(n, 2, 0);
    for (int fault = 1; fault <= 12; ++fault) exercise<fmmatrix::Law::DelayGraph>(2, 2, fault);
    for (int fault = 1; fault <= 9; ++fault) {
        exercise<fmmatrix::Law::In>(2, 2, fault);
        exercise<fmmatrix::Law::NL>(2, 2, fault);
        if (fault != 9) exercise<fmmatrix::Law::Wave>(2, 2, fault);
    }
    for (int fault = 1; fault <= 7; ++fault) {
        exercise<fmmatrix::Law::PM>(2, 2, fault);
        exercise<fmmatrix::Law::FM>(2, 2, fault);
        exercise<fmmatrix::Law::ZDF>(2, 2, fault);
        exercise<fmmatrix::Law::ExpFM>(2, 2, fault);
    }
    for (int repetition = 0; repetition < 1000; ++repetition) exercise<fmmatrix::Law::PM>(3, 1, 0);
    std::cout << "FMMatrixUGens server adapter tests passed\n";
}
