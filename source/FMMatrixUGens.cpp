// SPDX-License-Identifier: GPL-3.0-or-later
#include "SC_PlugIn.h"
#include "MatrixLayout.hpp"
#include "MatrixDSP.hpp"
#include "ControlSmoothing.hpp"
#include "MatrixEngine.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>

InterfaceTable* ft = nullptr;

namespace {

struct PMMatrix : Unit {
    double* state = nullptr;
    double *values = nullptr, *start = nullptr, *target = nullptr, *smoothed = nullptr, *reset = nullptr;
    double* previousValues = nullptr;
    fmmatrix::MatrixEngine* engine = nullptr;
    unsigned iterations = 4;
    std::uint32_t* graphIndices = nullptr;
    std::size_t edges = 0, depthBegin = 0, depthEnd = 0;
    std::size_t externalCount = 0;
    double* delayMemory = nullptr;
    std::size_t delayLength = 0, delayPosition = 0;
    double maxDelaySamples = 0;
    std::uint32_t operators = 0U;
    bool valid = false;
};
using FMMatrix = PMMatrix;
using FMMatrixExp = PMMatrix;
using PMMatrixZDF = PMMatrix;
using PMGraph = PMMatrix;
using PMMatrixWave = PMMatrix;
using PMMatrixIn = PMMatrix;
using PMDelayGraph = PMMatrix;
using PMMatrixNL = PMMatrix;

double bufferWave(PMMatrix* unit, double number, double phase) noexcept
{
    if (!std::isfinite(number) || number < 0 || std::floor(number) != number
        || number > std::numeric_limits<std::uint32_t>::max()) return 0;
    const auto index = static_cast<std::uint32_t>(number);
    SndBuf* buf = nullptr;
    if (index < unit->mWorld->mNumSndBufs) {
        if (unit->mWorld->mSndBufs) buf = unit->mWorld->mSndBufs + index;
    } else if (unit->mParent && unit->mParent->mLocalSndBufs && unit->mParent->localBufNum >= 0) {
        const auto local = index - unit->mWorld->mNumSndBufs;
        if (local <= static_cast<std::uint32_t>(unit->mParent->localBufNum)) buf = unit->mParent->mLocalSndBufs + local;
    }
    if (!buf) return 0;
    LOCK_SNDBUF_SHARED(buf);
    if (buf->channels != 1 || buf->frames < 2) return 0;
    return fmmatrix::cycleTable(buf->data, static_cast<std::size_t>(buf->frames), phase);
}

bool operatorCount(Unit* unit, std::uint32_t& result) noexcept
{
    if (unit->mNumInputs < 1U)
        return false;
    const double value = static_cast<double>(IN0(0));
    if (!std::isfinite(value) || value < 1.0
        || value > static_cast<double>(std::numeric_limits<std::uint32_t>::max())
        || std::floor(value) != value)
        return false;
    result = static_cast<std::uint32_t>(value);
    return true;
}

void clear(PMMatrix* unit, int inNumSamples)
{
    for (std::uint32_t output = 0U; output < unit->mNumOutputs; ++output)
        std::fill(OUT(static_cast<int>(output)), OUT(static_cast<int>(output)) + inNumSamples, 0.0F);
}

template <fmmatrix::Law law> void Matrix_next(PMMatrix* unit, int inNumSamples)
{
    if (!unit->valid) { clear(unit, inNumSamples); return; }
    auto& engine = *unit->engine;
    const std::size_t n = unit->operators;
    const std::size_t matrixBegin = unit->depthBegin, matrixEnd = unit->depthEnd;
    const auto extraBegin = matrixEnd + 2*n + 2;
    const double lagAmount = fmmatrix::lagAmount(IN0(extraBegin - 1), SAMPLERATE);
    fmmatrix::DelayHistory delays(unit->delayMemory, n, unit->delayLength, unit->maxDelaySamples);
    delays.position = unit->delayPosition;
    for (std::size_t i = matrixBegin; i < matrixEnd; ++i) {
        unit->start[i] = unit->target[i];
        unit->target[i] = fmmatrix::finite(IN0(i));
    }
    for (int frame = 0; frame < inNumSamples; ++frame) {
        const double fraction = static_cast<double>(frame + 1) / inNumSamples;
        for (std::size_t i = 1; i < unit->mNumInputs; ++i) {
            double value = fmmatrix::finite(IN(i)[INRATE(i) == calc_FullRate ? frame : 0]);
            if (i >= matrixBegin && i < matrixEnd) {
                if (INRATE(i) == calc_BufRate)
                    value = fmmatrix::ramp(unit->start[i], unit->target[i], fraction);
                unit->smoothed[i] = fmmatrix::lag(unit->smoothed[i], value, lagAmount);
                value = unit->smoothed[i];
            }
            unit->values[i] = value;
        }
        for (std::size_t i = 0; i < n; ++i)
            fmmatrix::resetPhase(engine.dsp.phase[i], unit->reset[i],
                unit->values[matrixEnd + i], unit->values[matrixEnd + n + i]);
        auto read = [&](std::size_t i, unsigned sub, unsigned factor) {
            if (factor == 1) return unit->values[i];
            return fmmatrix::ramp(unit->previousValues[i], unit->values[i],
                static_cast<double>(sub + 1) / factor);
        };
        if constexpr (law == fmmatrix::Law::Graph) {
            engine.tickGraph(read, unit->graphIndices, unit->edges ? unit->graphIndices + unit->edges : nullptr,
                unit->edges, matrixBegin, SAMPLERATE);
        } else if constexpr (law == fmmatrix::Law::DelayGraph) {
            engine.tickDelayedGraph(read, unit->graphIndices, unit->edges ? unit->graphIndices + unit->edges : nullptr,
                unit->edges, matrixBegin, extraBegin, delays, SAMPLERATE);
        } else if constexpr (law == fmmatrix::Law::Wave || law == fmmatrix::Law::In || law == fmmatrix::Law::NL) {
            auto evaluate = [&](std::size_t i, double phase, double sum) {
                if constexpr (law == fmmatrix::Law::Wave) return bufferWave(unit, IN0(extraBegin+i), phase+sum);
                else if constexpr (law == fmmatrix::Law::NL)
                    return std::sin(fmmatrix::wrap(phase + fmmatrix::nonlinear(sum, static_cast<unsigned>(IN0(extraBegin+i)))));
                else return std::sin(fmmatrix::wrap(phase+sum));
            };
            engine.tickExtended(read, evaluate, matrixBegin, unit->externalCount, 2+3*n, SAMPLERATE);
        } else {
            engine.tick<law>(read, SAMPLERATE, unit->iterations,
                law == fmmatrix::Law::ZDF ? unit->values[unit->mNumInputs - 1] : 1);
        }
        std::copy(unit->values, unit->values + unit->mNumInputs, unit->previousValues);
        for (std::uint32_t i = 0; i < unit->operators; ++i)
            OUT(i)[frame] = static_cast<float>(std::clamp(fmmatrix::finite(engine.output[i]),
                -static_cast<double>(std::numeric_limits<float>::max()),
                static_cast<double>(std::numeric_limits<float>::max())));
    }
    unit->delayPosition = delays.position;
}

template <fmmatrix::Law law> void Matrix_Ctor(PMMatrix* unit)
{
    unit->state = nullptr;
    unit->engine = nullptr;
    unit->graphIndices = nullptr;
    unit->delayMemory = nullptr;
    unit->delayLength = unit->delayPosition = unit->externalCount = 0;
    unit->maxDelaySamples = 0;
    unit->edges = 0;
    unit->iterations = 4;
    unit->operators = 0U;
    unit->valid = false;
    SETCALC(Matrix_next<law>);

    std::uint32_t operators = 0U;
    const bool countValid = operatorCount(unit, operators);
    const std::size_t extra = law == fmmatrix::Law::ZDF ? 2
        : (law == fmmatrix::Law::Wave || law == fmmatrix::Law::NL) ? operators : 0;
    bool layoutValid = countValid && fmmatrix::isValidLayout(
        operators, static_cast<std::uint64_t>(unit->mNumInputs),
        static_cast<std::uint64_t>(unit->mNumOutputs), extra);
    unit->depthBegin = 1 + 3 * static_cast<std::size_t>(operators);
    unit->depthEnd = unit->depthBegin + static_cast<std::size_t>(operators) * operators;
    if constexpr (law == fmmatrix::Law::Graph || law == fmmatrix::Law::DelayGraph) {
        const auto index = unit->depthBegin;
        layoutValid = countValid && index < unit->mNumInputs;
        if (layoutValid) {
            const double edgeCount = IN0(index);
            layoutValid = INRATE(index) == calc_ScalarRate && std::isfinite(edgeCount)
                && edgeCount >= 0 && edgeCount <= std::numeric_limits<std::uint32_t>::max()
                && std::floor(edgeCount) == edgeCount;
            if (layoutValid) {
                unit->edges = static_cast<std::size_t>(edgeCount);
                std::uint64_t required = 0;
                layoutValid = fmmatrix::graphInputCount(operators, unit->edges, required);
                if constexpr (law == fmmatrix::Law::DelayGraph) required += unit->edges+1;
                layoutValid = layoutValid && required == unit->mNumInputs && unit->mNumOutputs == operators;
            }
            if (layoutValid) for (std::size_t edge = 0; edge < 2 * unit->edges; ++edge) {
                const double node = IN0(index + 1 + edge);
                if (INRATE(index + 1 + edge) != calc_ScalarRate || !std::isfinite(node)
                    || node < 0 || node >= operators || std::floor(node) != node) { layoutValid = false; break; }
            }
        }
        unit->depthBegin = index + 1 + 2 * unit->edges;
        unit->depthEnd = unit->depthBegin + unit->edges;
    }
    if constexpr (law == fmmatrix::Law::In) {
        const auto index = unit->depthBegin;
        layoutValid = countValid && index < unit->mNumInputs;
        if (layoutValid) {
            const double k = IN0(index);
            layoutValid = INRATE(index) == calc_ScalarRate && std::isfinite(k) && k >= 0
                && k <= std::numeric_limits<std::uint32_t>::max() && std::floor(k) == k;
            if (layoutValid) {
                unit->externalCount = static_cast<std::size_t>(k);
                std::uint64_t required = 0;
                layoutValid = fmmatrix::externalInputCount(operators, unit->externalCount, required)
                    && required == unit->mNumInputs && unit->mNumOutputs == operators;
            }
        }
        unit->depthBegin = index + 1 + unit->externalCount;
        unit->depthEnd = unit->depthBegin + operators * (static_cast<std::size_t>(operators) + unit->externalCount);
    }
    if (!layoutValid || INRATE(0) != calc_ScalarRate) {
        Print("FMMatrixUGens: malformed construction-time operator layout; output is silent.\n");
        Matrix_next<law>(unit, 1);
        return;
    }
    if constexpr (law == fmmatrix::Law::ZDF) {
        const std::size_t index = unit->mNumInputs - 2;
        const double iterations = IN0(index);
        if (INRATE(index) != calc_ScalarRate || !std::isfinite(iterations)
            || iterations < 1 || iterations > 64 || std::floor(iterations) != iterations) {
            Print("PMMatrixZDF: iterations must be a constant integer in 1..64; output is silent.\n");
            Matrix_next<law>(unit, 1); return;
        }
        unit->iterations = static_cast<unsigned>(iterations);
    }
    const auto extraBegin = unit->depthEnd + 2*operators + 2;
    if constexpr (law == fmmatrix::Law::Wave || law == fmmatrix::Law::NL) {
        for (std::size_t i = extraBegin; i < unit->mNumInputs; ++i) {
            bool valid = INRATE(i) != calc_FullRate;
            if constexpr (law == fmmatrix::Law::NL) {
                const double shape = IN0(i);
                valid = INRATE(i) == calc_ScalarRate && std::isfinite(shape)
                    && shape >= 0 && shape <= 7 && std::floor(shape) == shape;
            }
            if (!valid) {
                Print("FMMatrixUGens: invalid waveform/shape input rate or shape number; output is silent.\n");
                Matrix_next<law>(unit, 1); return;
            }
        }
    }
    const std::size_t oversampleInput = unit->depthEnd + 2*operators;
    const float rate = IN0(oversampleInput);
    if (INRATE(oversampleInput) != calc_ScalarRate || !(rate == 1 || rate == 2 || rate == 4 || rate == 8)) {
        Print("FMMatrixUGens: oversample must be the constant 1, 2, 4 or 8; output is silent.\n");
        Matrix_next<law>(unit, 1);
        return;
    }
    const unsigned factor = static_cast<unsigned>(rate);
    if constexpr (law == fmmatrix::Law::DelayGraph) {
        const auto index = unit->mNumInputs - 1;
        const double seconds = IN0(index);
        const double samples = std::max(static_cast<double>(factor), seconds * SAMPLERATE * factor);
        // Check before any floating-to-integer conversion or byte multiplication.
        const auto capacity = std::numeric_limits<std::size_t>::max() / sizeof(double) / operators;
        if (INRATE(index) != calc_ScalarRate || !std::isfinite(seconds) || seconds <= 0
            || !std::isfinite(samples) || samples >= static_cast<double>(capacity - 1)) {
            Print("PMDelayGraph: invalid or oversized maxDelay; output is silent.\n");
            Matrix_next<law>(unit, 1); return;
        }
        unit->maxDelaySamples = samples;
        unit->delayLength = static_cast<std::size_t>(std::ceil(samples)) + 1;
    }
    const std::size_t engineSize = fmmatrix::MatrixEngine::memorySize(operators, factor);
    const std::size_t allocationCount = engineSize + operators + 5 * static_cast<std::size_t>(unit->mNumInputs);
    if (allocationCount > std::numeric_limits<std::size_t>::max() / sizeof(double)) {
        Matrix_next<law>(unit, 1); return;
    }

    unit->state = static_cast<double*>(RTAlloc(unit->mWorld,
        allocationCount * sizeof(double)));
    if (!unit->state) {
        Print("FMMatrixUGens: real-time state allocation failed; output is silent.\n");
        Matrix_next<law>(unit, 1);
        return;
    }
    unit->operators = operators;
    std::fill(unit->state, unit->state + allocationCount, 0.0);
    unit->engine = static_cast<fmmatrix::MatrixEngine*>(RTAlloc(unit->mWorld, sizeof(fmmatrix::MatrixEngine)));
    if (!unit->engine) {
        Print("FMMatrixUGens: real-time engine allocation failed; output is silent.\n");
        Matrix_next<law>(unit, 1); return;
    }
    new (unit->engine) fmmatrix::MatrixEngine(operators, factor, unit->state);
    if constexpr (law == fmmatrix::Law::Graph || law == fmmatrix::Law::DelayGraph) {
        if (unit->edges > 0) {
            unit->graphIndices = static_cast<std::uint32_t*>(RTAlloc(unit->mWorld, 2 * unit->edges * sizeof(std::uint32_t)));
            if (!unit->graphIndices) {
                Print("PMGraph: real-time topology allocation failed; output is silent.\n");
                Matrix_next<law>(unit, 1); return;
            }
            for (std::size_t edge = 0; edge < 2 * unit->edges; ++edge)
                unit->graphIndices[edge] = static_cast<std::uint32_t>(IN0(2 + 3 * operators + edge));
        }
    }
    if constexpr (law == fmmatrix::Law::DelayGraph) {
        unit->delayMemory = static_cast<double*>(RTAlloc(unit->mWorld, operators * unit->delayLength * sizeof(double)));
        if (!unit->delayMemory) {
            Print("PMDelayGraph: real-time delay allocation failed; output is silent.\n");
            Matrix_next<law>(unit, 1); return;
        }
        std::fill(unit->delayMemory, unit->delayMemory + operators * unit->delayLength, 0);
    }
    unit->reset = unit->state + engineSize;
    unit->values = unit->reset + operators;
    unit->start = unit->values + unit->mNumInputs;
    unit->target = unit->start + unit->mNumInputs;
    unit->smoothed = unit->target + unit->mNumInputs;
    unit->previousValues = unit->smoothed + unit->mNumInputs;
    for (std::size_t i = 0; i < unit->mNumInputs; ++i)
        unit->values[i] = unit->start[i] = unit->target[i] = unit->smoothed[i] = unit->previousValues[i] = fmmatrix::finite(IN0(i));
    auto read = [&](std::size_t index) { return static_cast<double>(IN0(index)); };
    unit->engine->initialize(read);
    unit->valid = true;
    Matrix_next<law>(unit, 1);
    // Constructor preview must not consume the first audio sample.
    unit->engine->initialize(read);
    std::fill(unit->reset, unit->reset + operators, 0.0);
    if constexpr (law == fmmatrix::Law::DelayGraph) {
        std::fill(unit->delayMemory, unit->delayMemory + operators * unit->delayLength, 0);
        unit->delayPosition = 0;
    }
    for (std::size_t i = 0; i < unit->mNumInputs; ++i)
        unit->smoothed[i] = fmmatrix::finite(IN0(i));
}

void PMMatrix_Dtor(PMMatrix* unit)
{
    if (unit->delayMemory) { RTFree(unit->mWorld, unit->delayMemory); unit->delayMemory = nullptr; }
    if (unit->graphIndices) { RTFree(unit->mWorld, unit->graphIndices); unit->graphIndices = nullptr; }
    if (unit->engine) {
        unit->engine->~MatrixEngine();
        RTFree(unit->mWorld, unit->engine);
        unit->engine = nullptr;
    }
    if (unit->state) {
        RTFree(unit->mWorld, unit->state);
        unit->state = nullptr;
    }
    unit->operators = 0U;
    unit->valid = false;
}

void PMMatrix_Ctor(PMMatrix* unit) { Matrix_Ctor<fmmatrix::Law::PM>(unit); }
void FMMatrix_Ctor(FMMatrix* unit) { Matrix_Ctor<fmmatrix::Law::FM>(unit); }
void FMMatrix_Dtor(FMMatrix* unit) { PMMatrix_Dtor(unit); }
void FMMatrixExp_Ctor(FMMatrixExp* unit) { Matrix_Ctor<fmmatrix::Law::ExpFM>(unit); }
void FMMatrixExp_Dtor(FMMatrixExp* unit) { PMMatrix_Dtor(unit); }
void PMMatrixZDF_Ctor(PMMatrixZDF* unit) { Matrix_Ctor<fmmatrix::Law::ZDF>(unit); }
void PMMatrixZDF_Dtor(PMMatrixZDF* unit) { PMMatrix_Dtor(unit); }
void PMGraph_Ctor(PMGraph* unit) { Matrix_Ctor<fmmatrix::Law::Graph>(unit); }
void PMGraph_Dtor(PMGraph* unit) { PMMatrix_Dtor(unit); }
void PMMatrixWave_Ctor(PMMatrixWave* unit) { Matrix_Ctor<fmmatrix::Law::Wave>(unit); }
void PMMatrixWave_Dtor(PMMatrixWave* unit) { PMMatrix_Dtor(unit); }
void PMMatrixIn_Ctor(PMMatrixIn* unit) { Matrix_Ctor<fmmatrix::Law::In>(unit); }
void PMMatrixIn_Dtor(PMMatrixIn* unit) { PMMatrix_Dtor(unit); }
void PMDelayGraph_Ctor(PMDelayGraph* unit) { Matrix_Ctor<fmmatrix::Law::DelayGraph>(unit); }
void PMDelayGraph_Dtor(PMDelayGraph* unit) { PMMatrix_Dtor(unit); }
void PMMatrixNL_Ctor(PMMatrixNL* unit) { Matrix_Ctor<fmmatrix::Law::NL>(unit); }
void PMMatrixNL_Dtor(PMMatrixNL* unit) { PMMatrix_Dtor(unit); }

} // namespace

PluginLoad(FMMatrixUGens)
{
    ft = inTable;
    DefineDtorUnit(PMMatrix);
    DefineDtorUnit(FMMatrix);
    DefineDtorUnit(FMMatrixExp);
    DefineDtorUnit(PMMatrixZDF);
    DefineDtorUnit(PMGraph);
    DefineDtorUnit(PMMatrixWave);
    DefineDtorUnit(PMMatrixIn);
    DefineDtorUnit(PMDelayGraph);
    DefineDtorUnit(PMMatrixNL);
}
