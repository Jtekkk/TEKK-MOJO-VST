#pragma once
#include <JuceHeader.h>
#include <map>
#include <vector>

// PresetManager owns factory + user presets.
// Loading resets all params to defaults first, then applies overrides,
// so partial preset maps work correctly.
class PresetManager : public juce::ChangeBroadcaster
{
public:
    struct Preset
    {
        juce::String name;
        std::map<juce::String, float> values; // paramID -> actual value
    };

    explicit PresetManager(juce::AudioProcessorValueTreeState& apvts);

    // ---- Query ----
    int  numFactory() const { return static_cast<int>(factory.size()); }
    int  numUser()    const { return static_cast<int>(user.size()); }
    int  numTotal()   const { return numFactory() + numUser(); }
    int  currentIndex() const { return current; }

    juce::StringArray allNames() const;
    juce::String      currentName() const;

    // ---- Load / Save ----
    void loadPreset(int index);
    void saveUserPreset(const juce::String& name);
    void deleteUserPreset(int index); // index within user[], not total[]

    // ---- Disk ----
    juce::File userDir() const;
    void       refreshUserList();

private:
    juce::AudioProcessorValueTreeState& apvts;
    std::vector<Preset> factory;
    std::vector<Preset> user;
    int current = 0;

    void buildFactory();
    void applyPreset(const Preset& p);
};
