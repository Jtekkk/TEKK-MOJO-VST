#pragma once
#include <JuceHeader.h>

// Two APVTS state snapshots with Fabfilter-style switching:
// selecting a slot saves the current state into the slot you're
// leaving, then restores the target slot — so both slots always
// hold the last state you left them in.
class ABState
{
public:
    void init(juce::AudioProcessorValueTreeState& a)
    {
        apvts  = &a;
        stateA = a.copyState();
        stateB = a.copyState();
        active = 0;
    }

    // slot: 0 = A, 1 = B
    void select(int slot)
    {
        if (apvts == nullptr || slot == active) return;

        if (active == 0) stateA = apvts->copyState();
        else             stateB = apvts->copyState();

        active = slot;
        apvts->replaceState(active == 0 ? stateA : stateB);
    }

    // Overwrites the inactive slot with the current (live) state
    void copyActiveToOther()
    {
        if (apvts == nullptr) return;
        auto current = apvts->copyState();
        if (active == 0) stateB = current;
        else             stateA = current;
    }

    int getActive() const noexcept { return active; }

private:
    juce::AudioProcessorValueTreeState* apvts = nullptr;
    juce::ValueTree stateA, stateB;
    int active = 0;
};
