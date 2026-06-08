#pragma once
#include <JuceHeader.h>
#include "MojoLookAndFeel.h"

// One labeled knob + value readout
struct MojoKnob : juce::Component
{
    juce::Slider slider { juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox };
    juce::Label  label;
    juce::Label  value;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;

    MojoKnob(const juce::String& name, juce::AudioProcessorValueTreeState& apvts,
             const juce::String& paramID)
    {
        slider.setScrollWheelEnabled(true);
        slider.setDoubleClickReturnValue(true,
            apvts.getParameterRange(paramID).convertFrom0to1(
                apvts.getParameter(paramID)->getDefaultValue()));

        attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, paramID, slider);

        label.setText(name, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, MojoColors::textDim);
        label.setFont(juce::Font(9.5f));

        value.setJustificationType(juce::Justification::centred);
        value.setColour(juce::Label::textColourId, MojoColors::textPrimary);
        value.setFont(juce::Font(9.f));

        slider.onValueChange = [this, &apvts, paramID] {
            auto* p = apvts.getParameter(paramID);
            value.setText(p->getText(p->getValue(), 6), juce::dontSendNotification);
        };
        slider.onValueChange();

        addAndMakeVisible(slider);
        addAndMakeVisible(label);
        addAndMakeVisible(value);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        label .setBounds(b.removeFromBottom(14));
        value .setBounds(b.removeFromBottom(12));
        slider.setBounds(b);
    }
};

// One labeled combo box
struct MojoCombo : juce::Component
{
    juce::ComboBox combo;
    juce::Label    label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attach;

    MojoCombo(const juce::String& name, juce::AudioProcessorValueTreeState& apvts,
              const juce::String& paramID)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(paramID)))
            for (auto& s : choice->choices)
                combo.addItem(s, combo.getNumItems() + 1);

        attach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            apvts, paramID, combo);

        label.setText(name, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, MojoColors::textDim);
        label.setFont(juce::Font(9.5f));

        addAndMakeVisible(combo);
        addAndMakeVisible(label);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        label.setBounds(b.removeFromBottom(14));
        combo.setBounds(b.reduced(2, 4));
    }
};

//==============================================================================
// A titled panel with a bypass toggle and a row of controls
//==============================================================================
class StagePanel : public juce::Component
{
public:
    static constexpr int kHeaderH = 26;
    static constexpr int kBodyH   = 90;
    static constexpr int kTotalH  = kHeaderH + kBodyH + 4;

    StagePanel(const juce::String& title, juce::Colour headerColor,
               juce::AudioProcessorValueTreeState& apvts,
               const juce::String& bypassParamID)
        : color(headerColor)
    {
        titleText = title;

        bypass.setButtonText("BYP");
        bypass.setToggleable(true);
        bypassAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, bypassParamID, bypass);
        addAndMakeVisible(bypass);
    }

    // Add a knob — returns *this for chaining
    StagePanel& addKnob(const juce::String& name, juce::AudioProcessorValueTreeState& apvts,
                        const juce::String& paramID)
    {
        auto k = std::make_unique<MojoKnob>(name, apvts, paramID);
        addAndMakeVisible(*k);
        knobs.push_back(std::move(k));
        return *this;
    }

    StagePanel& addCombo(const juce::String& name, juce::AudioProcessorValueTreeState& apvts,
                         const juce::String& paramID)
    {
        auto c = std::make_unique<MojoCombo>(name, apvts, paramID);
        addAndMakeVisible(*c);
        combos.push_back(std::move(c));
        return *this;
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();

        // Panel background
        g.setColour(MojoColors::panelBg);
        g.fillRoundedRectangle(b, 5.f);
        g.setColour(MojoColors::panelBorder);
        g.drawRoundedRectangle(b.reduced(0.5f), 5.f, 1.f);

        // Header bar
        auto header = b.removeFromTop(kHeaderH);
        g.setColour(color);
        g.fillRoundedRectangle(header, 5.f);
        // Square off bottom corners of header
        g.fillRect(header.withTrimmedTop(5.f));

        g.setColour(MojoColors::textPrimary);
        g.setFont(juce::Font(12.f, juce::Font::bold));
        g.drawText(titleText, header.reduced(36, 0), juce::Justification::centred);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        auto header = b.removeFromTop(kHeaderH);
        bypass.setBounds(header.removeFromRight(36).reduced(4, 4));

        // Layout: combos first (fixed 60px wide), then knobs share remainder
        int nCombos = static_cast<int>(combos.size());
        int nKnobs  = static_cast<int>(knobs.size());
        int total   = nCombos + nKnobs;
        if (total == 0) return;

        b.reduce(4, 4);
        int slotW = b.getWidth() / std::max(1, total);

        int x = b.getX();
        for (auto& c : combos)
        {
            c->setBounds(x, b.getY(), slotW, b.getHeight());
            x += slotW;
        }
        for (auto& k : knobs)
        {
            k->setBounds(x, b.getY(), slotW, b.getHeight());
            x += slotW;
        }
    }

private:
    juce::String titleText;
    juce::Colour color;
    juce::ToggleButton bypass;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttach;
    std::vector<std::unique_ptr<MojoKnob>>  knobs;
    std::vector<std::unique_ptr<MojoCombo>> combos;
};
