#pragma once
#include <JuceHeader.h>

namespace MojoColors
{
    // Stage category colors (from signal flow diagram)
    const juce::Colour saturation  { 0xff7a4a00 }; // amber/gold
    const juce::Colour toneDyn     { 0xff1a6b4a }; // teal
    const juce::Colour utility     { 0xff4a4a4a }; // gray

    // UI chrome
    const juce::Colour background  { 0xff141414 };
    const juce::Colour panelBg     { 0xff1e1e1e };
    const juce::Colour panelBorder { 0xff2e2e2e };
    const juce::Colour textPrimary { 0xffe8e0d0 };
    const juce::Colour textDim     { 0xff888070 };
    const juce::Colour knobTrack   { 0xff333333 };
    const juce::Colour knobFill    { 0xffe8b84a }; // gold accent
    const juce::Colour bypass      { 0xffcc3333 };
    const juce::Colour bypassOff   { 0xff2a5c3a };
}

class MojoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MojoLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, MojoColors::background);
        setColour(juce::TextButton::buttonColourId,          MojoColors::panelBg);
        setColour(juce::TextButton::textColourOffId,         MojoColors::textPrimary);
        setColour(juce::ComboBox::backgroundColourId,        MojoColors::panelBg);
        setColour(juce::ComboBox::textColourId,              MojoColors::textPrimary);
        setColour(juce::ComboBox::outlineColourId,           MojoColors::panelBorder);
        setColour(juce::PopupMenu::backgroundColourId,       juce::Colour(0xff1a1a1a));
        setColour(juce::PopupMenu::textColourId,             MojoColors::textPrimary);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, MojoColors::saturation);
        setColour(juce::Label::textColourId,                 MojoColors::textPrimary);
        setColour(juce::ScrollBar::thumbColourId,            MojoColors::panelBorder);
    }

    // ---- Rotary knob ----
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& /*slider*/) override
    {
        auto bounds = juce::Rectangle<float>(x, y, w, h).reduced(4.f);
        float cx = bounds.getCentreX(), cy = bounds.getCentreY();
        float r  = bounds.getWidth() * 0.5f;

        // Track arc
        juce::Path track;
        track.addArc(cx - r, cy - r, r*2, r*2, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(MojoColors::knobTrack);
        g.strokePath(track, juce::PathStrokeType(3.f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        // Fill arc
        float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        juce::Path fill;
        fill.addArc(cx - r, cy - r, r*2, r*2, rotaryStartAngle, angle, true);
        g.setColour(MojoColors::knobFill);
        g.strokePath(fill, juce::PathStrokeType(3.f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        // Knob body
        float kr = r * 0.68f;
        g.setColour(juce::Colour(0xff282828));
        g.fillEllipse(cx - kr, cy - kr, kr*2, kr*2);
        g.setColour(MojoColors::panelBorder);
        g.drawEllipse(cx - kr, cy - kr, kr*2, kr*2, 1.f);

        // Pointer line
        float px = cx + (kr * 0.6f) * std::sin(angle);
        float py = cy - (kr * 0.6f) * std::cos(angle);
        g.setColour(MojoColors::textPrimary);
        g.drawLine(cx, cy, px, py, 2.f);
    }

    // ---- Linear slider (for mix/blend) ----
    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h,
                          float sliderPos, float /*minPos*/, float /*maxPos*/,
                          juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        if (style == juce::Slider::LinearHorizontal)
        {
            float trackY = y + h * 0.5f;
            g.setColour(MojoColors::knobTrack);
            g.fillRoundedRectangle(x, trackY - 2, w, 4, 2.f);
            g.setColour(MojoColors::knobFill);
            g.fillRoundedRectangle(x, trackY - 2, sliderPos - x, 4, 2.f);
            g.setColour(MojoColors::textPrimary);
            g.fillEllipse(sliderPos - 6, trackY - 6, 12, 12);
        }
        else
        {
            LookAndFeel_V4::drawLinearSlider(g, x, y, w, h, sliderPos, 0, 0, style, s);
        }
    }

    // ---- Toggle button (bypass) ----
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& btn,
                          bool /*highlighted*/, bool /*down*/) override
    {
        auto b = btn.getLocalBounds().toFloat().reduced(1.f);
        bool on = btn.getToggleState();
        g.setColour(on ? MojoColors::bypass : MojoColors::bypassOff);
        g.fillRoundedRectangle(b, 3.f);
        g.setColour(MojoColors::textPrimary);
        g.setFont(juce::Font(10.f, juce::Font::bold));
        g.drawText(btn.getButtonText(), b, juce::Justification::centred);
    }

    juce::Font getLabelFont(juce::Label&) override
    {
        return juce::Font(11.f);
    }
};
