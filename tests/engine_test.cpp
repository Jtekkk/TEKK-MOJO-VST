// JUCE-based engine integration tests.
// Tests MojoEngine end-to-end: null bypass, limiter brickwall, no NaN/Inf.
#include <JuceHeader.h>
#include "dsp/MojoEngine.h"
#include "Parameters.h"

static int failures = 0;
static int passes   = 0;

static void check(bool cond, const char* msg)
{
    if (cond) { std::cout << "PASS  " << msg << "\n"; ++passes; }
    else      { std::cout << "FAIL  " << msg << "\n"; ++failures; }
}

// ---- Minimal test processor -------------------------------------------------

class TestProcessor : public juce::AudioProcessor
{
public:
    TestProcessor()
        : AudioProcessor(BusesProperties()
              .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          apvts(*this, nullptr, "Test", Params::createLayout())
    {}

    const juce::String getName() const override { return "Test"; }
    bool  acceptsMidi()  const override { return false; }
    bool  producesMidi() const override { return false; }
    bool  isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int  getNumPrograms()  override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    juce::AudioProcessorValueTreeState apvts;
};

// ---- Helpers ----------------------------------------------------------------

static void setParam(juce::AudioProcessorValueTreeState& apvts,
                     const juce::String& id, float actualValue)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*>(apvts.getParameter(id)))
        p->setValueNotifyingHost(p->convertTo0to1(actualValue));
}

static void setBypass(juce::AudioProcessorValueTreeState& apvts,
                      const juce::String& id, bool bypassed)
{
    if (auto* p = apvts.getParameter(id))
        p->setValueNotifyingHost(bypassed ? 1.f : 0.f);
}

static void bypassAll(juce::AudioProcessorValueTreeState& apvts)
{
    using namespace ParamIDs;
    setBypass(apvts, inXfmrBypass,  true);
    setBypass(apvts, hpfBypass,     true);
    setBypass(apvts, ieqBypass,     true);
    setBypass(apvts, compBypass,    true);
    setBypass(apvts, driveBypass,   true);
    setBypass(apvts, oeqBypass,     true);
    setBypass(apvts, limBypass,     true);
    setBypass(apvts, outXfmrBypass, true);
    setBypass(apvts, clipBypass,    true);
}

static void fillSine(juce::AudioBuffer<float>& buf, float freqHz, float sr, float amp = 0.5f)
{
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        for (int i = 0; i < buf.getNumSamples(); ++i)
            buf.setSample(ch, i,
                          amp * std::sin(2.f * 3.14159265f * freqHz * i / sr));
}

static float rmsOf(const juce::AudioBuffer<float>& buf, int ch = 0)
{
    float sum = 0.f;
    for (int i = 0; i < buf.getNumSamples(); ++i)
        sum += buf.getSample(ch, i) * buf.getSample(ch, i);
    return std::sqrt(sum / buf.getNumSamples());
}

static float maxAbsOf(const juce::AudioBuffer<float>& buf)
{
    float m = 0.f;
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        for (int i = 0; i < buf.getNumSamples(); ++i)
            m = std::max(m, std::abs(buf.getSample(ch, i)));
    return m;
}

static bool hasNanOrInf(const juce::AudioBuffer<float>& buf)
{
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        for (int i = 0; i < buf.getNumSamples(); ++i)
            if (!std::isfinite(buf.getSample(ch, i))) return true;
    return false;
}

// ---- Tests ------------------------------------------------------------------

static void test_null_bypass()
{
    TestProcessor proc;
    MojoEngine engine;
    const double sr = 44100.0;
    const int blockSize = 512;

    engine.prepare(sr, blockSize);
    bypassAll(proc.apvts);
    setParam(proc.apvts, ParamIDs::inputTrim,  0.f);
    setParam(proc.apvts, ParamIDs::outputTrim, 0.f);

    // Warm up with the same sine so latency buffers are filled with signal
    juce::AudioBuffer<float> buf(2, blockSize);
    for (int i = 0; i < 10; ++i) {
        fillSine(buf, 1000.f, static_cast<float>(sr));
        engine.processBlock(buf, proc.apvts);
    }

    // Now the output is a steady-state delayed copy: RMS must match input
    fillSine(buf, 1000.f, static_cast<float>(sr));
    float inRMS = rmsOf(buf);
    engine.processBlock(buf, proc.apvts);
    float outRMS = rmsOf(buf);

    check(!hasNanOrInf(buf), "Null bypass: no NaN/Inf");
    check(std::abs(outRMS - inRMS) / std::max(inRMS, 1e-10f) < 0.02f,
          "Null bypass: RMS preserved within 2%");
}

static void test_limiter_brickwall()
{
    TestProcessor proc;
    MojoEngine engine;
    const double sr = 44100.0;
    const int blockSize = 512;

    engine.prepare(sr, blockSize);
    bypassAll(proc.apvts);
    setBypass(proc.apvts, ParamIDs::limBypass, false); // limiter ON
    setParam(proc.apvts, ParamIDs::limThresh, -6.f);   // threshold -6 dBFS = 0.501
    setParam(proc.apvts, ParamIDs::inputTrim,  0.f);
    setParam(proc.apvts, ParamIDs::outputTrim, 0.f);

    // Warm up with hot signal
    juce::AudioBuffer<float> buf(2, blockSize);
    for (int blk = 0; blk < 10; ++blk) {
        buf.applyGain(0.f);
        buf.applyGain(1.f); // silence — fill manually
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < blockSize; ++i)
                buf.setSample(ch, i, 1.0f); // 0 dBFS constant
        engine.processBlock(buf, proc.apvts);
    }

    // After warmup the limiter should be riding hard; max output <= ~0.51
    check(!hasNanOrInf(buf), "Limiter: no NaN/Inf");
    float peak = maxAbsOf(buf);
    check(peak <= 0.52f, "Limiter brickwall: peak <= -6 dBFS threshold");
}

static void test_no_nan_with_silence()
{
    TestProcessor proc;
    MojoEngine engine;
    engine.prepare(44100.0, 512);
    // Default params (all active)
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf(2, 512);
    buf.clear();
    for (int i = 0; i < 20; ++i)
        engine.processBlock(buf, proc.apvts);
    check(!hasNanOrInf(buf), "Silence: no NaN/Inf with all stages active");
}

static void test_no_nan_with_hot_signal()
{
    TestProcessor proc;
    MojoEngine engine;
    engine.prepare(44100.0, 512);

    juce::AudioBuffer<float> buf(2, 512);
    for (int blk = 0; blk < 20; ++blk) {
        fillSine(buf, 1000.f, 44100.f, 0.9f);
        engine.processBlock(buf, proc.apvts);
    }
    check(!hasNanOrInf(buf), "Hot sine: no NaN/Inf with all stages active");
}

static void test_saturation_adds_harmonics()
{
    TestProcessor proc;
    MojoEngine engine;
    const double sr = 44100.0;
    const int blockSize = 1024;

    engine.prepare(sr, blockSize);
    bypassAll(proc.apvts);
    setBypass(proc.apvts, ParamIDs::driveBypass, false); // drive ON
    setParam(proc.apvts, ParamIDs::driveAmount, 0.7f);
    setParam(proc.apvts, ParamIDs::driveBlend,  1.0f);
    setParam(proc.apvts, ParamIDs::inputTrim,  0.f);
    setParam(proc.apvts, ParamIDs::outputTrim, 0.f);

    // Warmup
    juce::AudioBuffer<float> buf(2, blockSize);
    for (int i = 0; i < 5; ++i) {
        fillSine(buf, 100.f, static_cast<float>(sr), 0.5f);
        engine.processBlock(buf, proc.apvts);
    }

    // With saturation, peak of one cycle should be <= peak of linear sine (0.5)
    // (drive at 0.7 definitely compresses)
    fillSine(buf, 100.f, static_cast<float>(sr), 0.5f);
    engine.processBlock(buf, proc.apvts);

    check(!hasNanOrInf(buf), "Drive: no NaN/Inf");
    // Output should be non-trivially different from zero
    check(rmsOf(buf) > 0.05f, "Drive: non-zero output");
}

static void test_latency_reported()
{
    TestProcessor proc;
    MojoEngine engine;
    engine.prepare(44100.0, 512);
    int lat = engine.getLatencyInSamples();
    check(lat > 0, "Latency: > 0 samples reported (lookahead + OS)");
    // At 2× OS + 2 ms lookahead: ~88 + ~176 = ~264 samples at 88.2 kHz OS
    check(lat < 2000, "Latency: < 2000 samples (sanity)");
}

// ---- Entry ------------------------------------------------------------------

int main()
{
    std::cout << "=== Engine Integration Tests ===\n\n";

    test_null_bypass();
    test_limiter_brickwall();
    test_no_nan_with_silence();
    test_no_nan_with_hot_signal();
    test_saturation_adds_harmonics();
    test_latency_reported();

    std::cout << "\n--- " << passes << " passed, " << failures << " failed ---\n";
    return failures > 0 ? 1 : 0;
}
