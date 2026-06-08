#include "MojoEngine.h"

int MojoEngine::getLatencyInSamples() const
{
    int osLatency  = oversampler
                     ? static_cast<int>(oversampler->getLatencyInSamples()) : 0;
    int limLatency = limL.latencyInOriginalSamples(osFactor);
    return osLatency + limLatency;
}

MojoEngine::MojoEngine()
{
    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
}

void MojoEngine::rebuildOversampler()
{
    int order = 0;
    if      (osFactor == 2) order = 1;
    else if (osFactor == 4) order = 2;
    else if (osFactor == 8) order = 3;

    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        2, order,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    oversampler->initProcessing(static_cast<size_t>(blockSize));
}

void MojoEngine::prepare(double sampleRate, int samplesPerBlock)
{
    sr        = sampleRate;
    blockSize = samplesPerBlock;

    rebuildOversampler();

    double osSR = sr * osFactor;
    int    osBS = blockSize * osFactor;

    auto prep = [&](auto& L, auto& R) { L.prepare(osSR, osBS); R.prepare(osSR, osBS); };
    prep(inXL, inXR); prep(hpfL, hpfR); prep(ieqL, ieqR);
    prep(cmpL, cmpR); prep(drvL, drvR); prep(oeqL, oeqR);
    prep(limL, limR); prep(otxL, otxR); prep(clpL, clpR);
}

void MojoEngine::reset()
{
    auto rst = [](auto& L, auto& R) { L.reset(); R.reset(); };
    rst(inXL, inXR); rst(hpfL, hpfR); rst(ieqL, ieqR);
    rst(cmpL, cmpR); rst(drvL, drvR); rst(oeqL, oeqR);
    rst(limL, limR); rst(otxL, otxR); rst(clpL, clpR);
    if (oversampler) oversampler->reset();
}

void MojoEngine::updateParams(juce::AudioProcessorValueTreeState& apvts)
{
    using namespace ParamIDs;
    auto fv = [&](const juce::String& id) { return apvts.getRawParameterValue(id)->load(); };
    auto bv = [&](const juce::String& id) { return fv(id) > 0.5f; };
    auto iv = [&](const juce::String& id) { return static_cast<int>(fv(id)); };

    inTrim  = std::pow(10.f, fv(inputTrim)  / 20.f);
    outTrim = std::pow(10.f, fv(outputTrim) / 20.f);
    agcOn   = bv(autoGain);

    int newOS = 1 << iv(oversample);
    if (newOS != osFactor)
    {
        osFactor = newOS;
        rebuildOversampler();
        double osSR = sr * osFactor;
        int    osBS = blockSize * osFactor;
        auto prep = [&](auto& L, auto& R) { L.prepare(osSR, osBS); R.prepare(osSR, osBS); };
        prep(inXL, inXR); prep(hpfL, hpfR); prep(ieqL, ieqR);
        prep(cmpL, cmpR); prep(drvL, drvR); prep(oeqL, oeqR);
        prep(limL, limR); prep(otxL, otxR); prep(clpL, clpR);
    }

    // Input Transformer
    auto cfgIT = [&](auto& s) {
        s.bypass   = bv(inXfmrBypass);
        s.type     = iv(inXfmrType);
        s.drive    = fv(inXfmrDrive);
        s.trimGain = std::pow(10.f, fv(inXfmrTrim) / 20.f);
        s.useAGC   = agcOn;
    };
    cfgIT(inXL); cfgIT(inXR);

    // HPF
    auto cfgHPF = [&](auto& s) {
        s.bypass = bv(hpfBypass);
        s.freq   = fv(hpfFreq);
        s.order  = iv(hpfOrder);
    };
    cfgHPF(hpfL); cfgHPF(hpfR);

    // Induction EQ
    auto cfgIEQ = [&](auto& s) {
        s.bypass   = bv(ieqBypass);
        s.lowFreq  = fv(ieqLowFreq);  s.lowGain  = fv(ieqLowGain);
        s.midFreq  = fv(ieqMidFreq);  s.midGain  = fv(ieqMidGain); s.midQ = fv(ieqMidQ);
        s.highFreq = fv(ieqHighFreq); s.highGain = fv(ieqHighGain);
        s.satDrive = fv(ieqSatDrive);
    };
    cfgIEQ(ieqL); cfgIEQ(ieqR);

    // Compressor
    auto cfgCmp = [&](auto& s) {
        s.bypass     = bv(compBypass);
        s.type       = iv(compType);
        s.threshold  = fv(compThresh);
        s.ratio      = fv(compRatio);
        s.attackMs   = fv(compAttack);
        s.releaseMs  = fv(compRelease);
        s.makeupDB   = fv(compMakeup);
        s.mix        = fv(compMix);
    };
    cfgCmp(cmpL); cfgCmp(cmpR);

    // Drive
    auto cfgDrv = [&](auto& s) {
        s.bypass  = bv(driveBypass);
        s.mode    = iv(driveMode);
        s.amount  = fv(driveAmount);
        s.blend   = fv(driveBlend);
        s.trimDB  = fv(driveTrim);
        s.useAGC  = agcOn;
    };
    cfgDrv(drvL); cfgDrv(drvR);

    // Output EQ
    auto cfgOEQ = [&](auto& s) {
        s.bypass   = bv(oeqBypass);
        s.lowFreq  = fv(oeqLowFreq);  s.lowGain  = fv(oeqLowGain);
        s.midFreq  = fv(oeqMidFreq);  s.midGain  = fv(oeqMidGain); s.midQ = fv(oeqMidQ);
        s.highFreq = fv(oeqHighFreq); s.highGain = fv(oeqHighGain);
    };
    cfgOEQ(oeqL); cfgOEQ(oeqR);

    // Limiter
    auto cfgLim = [&](auto& s) {
        s.bypass    = bv(limBypass);
        s.threshDB  = fv(limThresh);
        s.releaseMs = fv(limRelease);
    };
    cfgLim(limL); cfgLim(limR);

    // Output Transformer
    auto cfgOT = [&](auto& s) {
        s.bypass   = bv(outXfmrBypass);
        s.type     = iv(outXfmrType);
        s.drive    = fv(outXfmrDrive);
        s.trimDB   = fv(outXfmrTrim);
        s.useAGC   = agcOn;
    };
    cfgOT(otxL); cfgOT(otxR);

    // Safety Clip
    auto cfgClp = [&](auto& s) {
        s.bypass  = bv(clipBypass);
        s.levelDB = fv(clipLevel);
    };
    cfgClp(clpL); cfgClp(clpR);
}

void MojoEngine::processBlock(juce::AudioBuffer<float>& buffer,
                               juce::AudioProcessorValueTreeState& apvts)
{
    updateParams(apvts);

    const int numCh      = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // Global input trim
    for (int ch = 0; ch < numCh; ++ch)
        juce::FloatVectorOperations::multiply(buffer.getWritePointer(ch), inTrim, numSamples);

    // Input peak meter (post-trim, pre-OS)
    {
        auto writePeak = [](std::atomic<float>& atom, const float* data, int n) {
            float pk = 0.f;
            for (int i = 0; i < n; ++i) pk = std::max(pk, std::abs(data[i]));
            float old = atom.load(std::memory_order_relaxed);
            while (pk > old && !atom.compare_exchange_weak(old, pk, std::memory_order_relaxed)) {}
        };
        writePeak(inputMeter.peakL, buffer.getReadPointer(0), numSamples);
        writePeak(inputMeter.peakR,
                  numCh > 1 ? buffer.getReadPointer(1) : buffer.getReadPointer(0), numSamples);
    }

    // Upsample
    juce::dsp::AudioBlock<float> inBlock(buffer);
    auto osBlock = oversampler->processSamplesUp(inBlock);
    const int osN = static_cast<int>(osBlock.getNumSamples());

    // Left channel
    {
        float* L = osBlock.getChannelPointer(0);
        for (int i = 0; i < osN; ++i)
        {
            float x = L[i];
            x = inXL.process(x);
            x = hpfL.process(x);
            x = ieqL.process(x);
            x = cmpL.process(x);
            x = drvL.process(x);
            x = oeqL.process(x);
            x = limL.process(x);
            x = otxL.process(x);
            x = clpL.process(x);
            L[i] = x;
        }
    }

    // Right channel (fall back to same-as-left processing when mono)
    if (osBlock.getNumChannels() > 1)
    {
        float* R = osBlock.getChannelPointer(1);
        for (int i = 0; i < osN; ++i)
        {
            float x = R[i];
            x = inXR.process(x);
            x = hpfR.process(x);
            x = ieqR.process(x);
            x = cmpR.process(x);
            x = drvR.process(x);
            x = oeqR.process(x);
            x = limR.process(x);
            x = otxR.process(x);
            x = clpR.process(x);
            R[i] = x;
        }
    }

    // Compressor GR meter (sampled from last OS frame of each channel)
    {
        float grLin = (cmpL.lastGR + cmpR.lastGR) * 0.5f;
        compGRdB.store(20.f * std::log10(std::max(grLin, 1e-10f)),
                       std::memory_order_relaxed);
    }

    // Downsample
    oversampler->processSamplesDown(inBlock);

    // Global output trim
    for (int ch = 0; ch < numCh; ++ch)
        juce::FloatVectorOperations::multiply(buffer.getWritePointer(ch), outTrim, numSamples);

    // Output peak meter (post-trim)
    {
        auto writePeak = [](std::atomic<float>& atom, const float* data, int n) {
            float pk = 0.f;
            for (int i = 0; i < n; ++i) pk = std::max(pk, std::abs(data[i]));
            float old = atom.load(std::memory_order_relaxed);
            while (pk > old && !atom.compare_exchange_weak(old, pk, std::memory_order_relaxed)) {}
        };
        writePeak(outputMeter.peakL, buffer.getReadPointer(0), numSamples);
        writePeak(outputMeter.peakR,
                  numCh > 1 ? buffer.getReadPointer(1) : buffer.getReadPointer(0), numSamples);
    }
}
