#pragma once
#include <JuceHeader.h>
#include <atomic>
#include "MojoLookAndFeel.h"

//==============================================================================
// Filled by the audio thread (relaxed atomic store once per block).
// Read + reset by the UI thread (exchange) at ~30 Hz.
//==============================================================================
struct MeterSource
{
    std::atomic<float> peakL { 0.f };
    std::atomic<float> peakR { 0.f };
};

//==============================================================================
// Stereo peak meter with ballistic decay and peak-hold.
// Two bars side-by-side; click anywhere to reset clip indicators.
//==============================================================================
class LevelMeter : public juce::Component, private juce::Timer
{
public:
    LevelMeter()  { startTimerHz(30); }
    ~LevelMeter() { stopTimer(); }

    void setSource(MeterSource* src) { source = src; }

    void mouseDown(const juce::MouseEvent&) override
    {
        clipL = clipR = false;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        float barW  = (b.getWidth() - kGap) * 0.5f;
        float barsH = b.getHeight() - kClipH - kClipGap;

        auto drawBar = [&](float x, float dispDB, float pkDB, bool& clip)
        {
            // Background
            g.setColour(MojoColors::knobTrack);
            g.fillRect(x, kClipH + kClipGap, barW, barsH);

            // Level fill — gradient green→yellow→red
            float fillH = dbToFrac(dispDB) * barsH;
            if (fillH > 0.f)
            {
                juce::ColourGradient grad(
                    juce::Colour(0xff22cc44), x, b.getHeight(),
                    juce::Colour(0xffcc3322), x, kClipH + kClipGap, false);
                grad.addColour(0.7, juce::Colour(0xffddcc22));
                g.setGradientFill(grad);
                g.fillRect(x, kClipH + kClipGap + (barsH - fillH), barW, fillH);
            }

            // Peak hold line
            if (pkDB > kMinDB)
            {
                float py = kClipH + kClipGap + barsH * (1.f - dbToFrac(pkDB));
                g.setColour(juce::Colours::white.withAlpha(0.85f));
                g.fillRect(x, py - 1.f, barW, 2.f);
            }

            // Clip indicator
            g.setColour(clip ? juce::Colour(0xffff2222) : MojoColors::knobTrack);
            g.fillRect(x, 0.f, barW, kClipH);
        };

        drawBar(b.getX(),            displayL, peakL, clipL);
        drawBar(b.getX() + barW + kGap, displayR, peakR, clipR);
    }

    void resized() override {}

private:
    void timerCallback() override
    {
        if (!source) return;

        float rawL = source->peakL.exchange(0.f, std::memory_order_relaxed);
        float rawR = source->peakR.exchange(0.f, std::memory_order_relaxed);
        float dbL  = 20.f * std::log10(rawL + 1e-10f);
        float dbR  = 20.f * std::log10(rawR + 1e-10f);

        auto applyBallistic = [&](float raw, float& disp, float& pk,
                                  int& hold, bool& clip)
        {
            if (raw >= 0.f) clip = true;
            if (raw > disp)
                disp = raw;
            else
                disp = std::max(disp - kReleaseDBperFrame, kMinDB);

            if (raw >= pk) { pk = raw; hold = 0; }
            else if (++hold > kPeakHoldFrames) pk -= kReleaseDBperFrame * 0.3f;
            pk = std::max(pk, kMinDB);
        };

        applyBallistic(dbL, displayL, peakL, holdL, clipL);
        applyBallistic(dbR, displayR, peakR, holdR, clipR);
        repaint();
    }

    static float dbToFrac(float db)
    {
        return std::clamp((db - kMinDB) / (kMaxDB - kMinDB), 0.f, 1.f);
    }

    MeterSource* source = nullptr;

    float displayL = kMinDB, displayR = kMinDB;
    float peakL    = kMinDB, peakR    = kMinDB;
    int   holdL = 0, holdR = 0;
    bool  clipL = false, clipR = false;

    static constexpr float kMinDB            = -60.f;
    static constexpr float kMaxDB            =   0.f;
    static constexpr float kReleaseDBperFrame =  0.4f; // ~12 dB/s at 30 fps
    static constexpr int   kPeakHoldFrames   = 90;     // 3 s hold
    static constexpr float kClipH            =  4.f;
    static constexpr float kClipGap          =  2.f;
    static constexpr float kGap              =  2.f;
};
