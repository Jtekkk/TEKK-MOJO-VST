#pragma once
#include <JuceHeader.h>
#include "Parameters.h"
#include "dsp/MojoEngine.h"
#include "PresetManager.h"
#include "ABState.h"

class TekkMojoProcessor : public juce::AudioProcessor
{
public:
    TekkMojoProcessor();
    ~TekkMojoProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Programs map 1:1 to factory presets for DAW preset browser support
    int  getNumPrograms()    override { return presets.numFactory(); }
    int  getCurrentProgram() override { return presets.currentIndex(); }
    void setCurrentProgram(int index) override { presets.loadPreset(index); }
    const juce::String getProgramName(int index) override
    {
        auto names = presets.allNames();
        return index < names.size() ? names[index] : juce::String{};
    }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;
    ABState       abState;

    MeterSource&        inputMeter()  { return engine.inputMeter; }
    MeterSource&        outputMeter() { return engine.outputMeter; }
    std::atomic<float>& compGRdB()   { return engine.compGRdB; }

private:
    MojoEngine engine;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TekkMojoProcessor)
};
