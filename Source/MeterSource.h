#pragma once
#include <atomic>

// Written by audio thread, read by UI thread.
// Lives here (not in UI/) so DSP code can include it without pulling in any
// LookAndFeel or Component headers.
struct MeterSource
{
    std::atomic<float> peakL { 0.f };
    std::atomic<float> peakR { 0.f };
};
