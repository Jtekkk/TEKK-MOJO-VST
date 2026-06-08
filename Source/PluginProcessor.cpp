#include "PluginProcessor.h"
#include "PluginEditor.h"

TekkMojoProcessor::TekkMojoProcessor()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "TekkMojoState", Params::createLayout()),
      presets(apvts)
{
    abState.init(apvts);
}

void TekkMojoProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engine.prepare(sampleRate, samplesPerBlock);
    setLatencySamples(engine.getLatencyInSamples());
}

void TekkMojoProcessor::releaseResources()
{
    engine.reset();
}

void TekkMojoProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;
    engine.processBlock(buffer, apvts);
}

juce::AudioProcessorEditor* TekkMojoProcessor::createEditor()
{
    return new TekkMojoEditor(*this);
}

void TekkMojoProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void TekkMojoProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TekkMojoProcessor();
}
