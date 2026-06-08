#pragma once
#include "Stages.h"
#include "../Parameters.h"
#include "../MeterSource.h"
#include <JuceHeader.h>
#include <memory>
#include <atomic>

class MojoEngine
{
public:
    MojoEngine();

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();
    void processBlock(juce::AudioBuffer<float>& buffer,
                      juce::AudioProcessorValueTreeState& apvts);

    // Total plugin latency (oversampler + limiter lookahead) in original samples.
    int getLatencyInSamples() const;

    // Meter data — written by audio thread, read by UI thread.
    MeterSource            inputMeter, outputMeter;
    std::atomic<float>     compGRdB { 0.f }; // negative dB (e.g. -6)

private:
    void updateParams(juce::AudioProcessorValueTreeState& apvts);
    void rebuildOversampler();

    double sr        = 44100.0;
    int    blockSize = 512;
    int    osFactor  = 2;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    // Stereo stage pairs
    Stages::InputTransformer  inXL,  inXR;
    Stages::HighPassFilter    hpfL,  hpfR;
    Stages::InductionEQ       ieqL,  ieqR;
    Stages::Compressor        cmpL,  cmpR;
    Stages::TubeTapeDrive     drvL,  drvR;
    Stages::OutputEQ          oeqL,  oeqR;
    Stages::Limiter           limL,  limR;
    Stages::OutputTransformer otxL,  otxR;
    Stages::SafetyClip        clpL,  clpR;

    float inTrim  = 1.f;
    float outTrim = 1.f;
    bool  agcOn   = true;

    juce::SmoothedValue<float> inTrimS, outTrimS;
};
