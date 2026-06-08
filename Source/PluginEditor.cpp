#include "PluginEditor.h"

TekkMojoEditor::TekkMojoEditor(TekkMojoProcessor& p)
    : AudioProcessorEditor(p), proc(p), genericEditor(p)
{
    addAndMakeVisible(genericEditor);
    setSize(620, 960);
    setResizable(true, true);
}

void TekkMojoEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1c1c1c));
}

void TekkMojoEditor::resized()
{
    genericEditor.setBounds(getLocalBounds());
}
