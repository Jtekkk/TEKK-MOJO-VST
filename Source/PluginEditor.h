#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class TekkMojoEditor : public juce::AudioProcessorEditor
{
public:
    explicit TekkMojoEditor(TekkMojoProcessor&);
    ~TekkMojoEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    TekkMojoProcessor& proc;
    juce::GenericAudioProcessorEditor genericEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TekkMojoEditor)
};
