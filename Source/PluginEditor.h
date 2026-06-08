#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/MojoLookAndFeel.h"
#include "UI/StagePanel.h"

class TekkMojoEditor : public juce::AudioProcessorEditor
{
public:
    explicit TekkMojoEditor(TekkMojoProcessor&);
    ~TekkMojoEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void buildStages();

    TekkMojoProcessor& proc;
    MojoLookAndFeel    laf;

    // Header / global strip
    juce::Label  pluginTitle;
    MojoCombo    osCombo;
    MojoKnob     inTrimKnob, outTrimKnob;
    juce::ToggleButton agcButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> agcAttach;

    // Signal-chain stage panels
    StagePanel panelInXfmr, panelHPF, panelIEQ, panelComp,
               panelDrive,  panelOEQ, panelLim, panelOutXfmr, panelClip;

    juce::Viewport  viewport;
    juce::Component stagesContainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TekkMojoEditor)
};
