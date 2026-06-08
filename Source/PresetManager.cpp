#include "PresetManager.h"
#include "Parameters.h"

namespace P = ParamIDs;

// ---- Shorthand for building preset maps ----
static std::map<juce::String, float> vals(
    std::initializer_list<std::pair<const juce::String, float>> il)
{ return { il.begin(), il.end() }; }

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& a) : apvts(a)
{
    buildFactory();
    refreshUserList();
}

// =============================================================================
// Factory presets — only non-default values are listed; applyPreset() resets
// everything to its default first, then applies these overrides.
// =============================================================================
void PresetManager::buildFactory()
{
    factory.clear();

    // 1. Init — all defaults, nothing listed
    factory.push_back({ "Init", {} });

    // 2. Tape Bus — warm tape bus glue
    factory.push_back({ "Tape Bus", vals({
        { P::driveMode,    1.f  },   // Tape
        { P::driveAmount,  0.45f},
        { P::driveBlend,   0.8f },
        { P::compType,     1.f  },   // Opto
        { P::compThresh,   -18.f},
        { P::compRatio,    2.0f },
        { P::compAttack,   30.f },
        { P::compRelease,  250.f},
        { P::compMakeup,   3.f  },
        { P::compMix,      0.85f},
        { P::inXfmrDrive,  0.25f},
        { P::outXfmrDrive, 0.2f },
        { P::ieqLowGain,   1.0f },
    })});

    // 3. Iron Vox — transformer color + FET on vocals
    factory.push_back({ "Iron Vox", vals({
        { P::inXfmrType,   1.f  },   // Iron II
        { P::inXfmrDrive,  0.52f},
        { P::outXfmrType,  1.f  },
        { P::outXfmrDrive, 0.35f},
        { P::compType,     0.f  },   // FET
        { P::compThresh,   -15.f},
        { P::compRatio,    3.f  },
        { P::compAttack,   5.f  },
        { P::compRelease,  80.f },
        { P::compMakeup,   4.f  },
        { P::ieqHighFreq,  10000.f},
        { P::ieqHighGain,  2.0f },
        { P::ieqLowGain,   1.5f },
        { P::hpfFreq,      80.f },
    })});

    // 4. FET Drums — fast, punchy, iron bite
    factory.push_back({ "FET Drums", vals({
        { P::compType,     0.f  },   // FET
        { P::compThresh,   -20.f},
        { P::compRatio,    6.f  },
        { P::compAttack,   1.5f },
        { P::compRelease,  55.f },
        { P::compMakeup,   6.f  },
        { P::hpfFreq,      60.f },
        { P::inXfmrDrive,  0.45f},
        { P::outXfmrDrive, 0.3f },
        { P::ieqMidFreq,   3000.f},
        { P::ieqMidGain,   1.5f },
        { P::ieqMidQ,      1.2f },
    })});

    // 5. Vintage Glue — Vari-Mu + Iron II, slow and musical
    factory.push_back({ "Vintage Glue", vals({
        { P::inXfmrType,   1.f  },   // Iron II
        { P::inXfmrDrive,  0.38f},
        { P::outXfmrType,  1.f  },
        { P::outXfmrDrive, 0.30f},
        { P::compType,     3.f  },   // Vari-Mu
        { P::compThresh,   -14.f},
        { P::compRatio,    3.0f },
        { P::compAttack,   50.f },
        { P::compRelease,  400.f},
        { P::compMakeup,   2.f  },
        { P::ieqLowGain,   1.0f },
        { P::ieqSatDrive,  0.3f },
    })});

    // 6. Tube Push — driven tube into VCA comp
    factory.push_back({ "Tube Push", vals({
        { P::driveMode,    0.f  },   // Tube
        { P::driveAmount,  0.65f},
        { P::driveBlend,   0.85f},
        { P::inXfmrDrive,  0.42f},
        { P::outXfmrDrive, 0.3f },
        { P::compType,     2.f  },   // VCA
        { P::compThresh,   -12.f},
        { P::compRatio,    3.f  },
        { P::compMakeup,   3.f  },
        { P::ieqLowGain,   1.0f },
    })});

    // 7. Dark & Thick — Iron II pushed hard, tape blend
    factory.push_back({ "Dark & Thick", vals({
        { P::inXfmrType,   1.f  },   // Iron II
        { P::inXfmrDrive,  0.68f},
        { P::outXfmrType,  1.f  },
        { P::outXfmrDrive, 0.52f},
        { P::driveMode,    1.f  },   // Tape
        { P::driveAmount,  0.5f },
        { P::driveBlend,   0.6f },
        { P::compType,     3.f  },   // Vari-Mu
        { P::compThresh,   -16.f},
        { P::compRatio,    2.5f },
        { P::compAttack,   60.f },
        { P::compRelease,  350.f},
        { P::ieqLowFreq,   120.f},
        { P::ieqLowGain,   2.5f },
        { P::ieqHighGain,  -2.0f},
        { P::ieqSatDrive,  0.4f },
    })});

    // 8. Hi-Fi Sheen — Iron III, air shelf, clean VCA
    factory.push_back({ "Hi-Fi Sheen", vals({
        { P::inXfmrType,   2.f  },   // Iron III
        { P::inXfmrDrive,  0.18f},
        { P::outXfmrType,  2.f  },
        { P::outXfmrDrive, 0.14f},
        { P::compType,     2.f  },   // VCA
        { P::compThresh,   -8.f },
        { P::compRatio,    2.f  },
        { P::compAttack,   8.f  },
        { P::compRelease,  120.f},
        { P::compMakeup,   2.f  },
        { P::oeqHighFreq,  12000.f},
        { P::oeqHighGain,  2.0f },
    })});

    // 9. Bus Glue — parallel Opto + tape blend
    factory.push_back({ "Bus Glue", vals({
        { P::compType,     1.f  },   // Opto
        { P::compThresh,   -24.f},
        { P::compRatio,    2.5f },
        { P::compAttack,   20.f },
        { P::compRelease,  300.f},
        { P::compMakeup,   4.f  },
        { P::compMix,      0.55f},
        { P::driveMode,    1.f  },   // Tape
        { P::driveAmount,  0.3f },
        { P::driveBlend,   0.5f },
        { P::inXfmrDrive,  0.22f},
        { P::outXfmrDrive, 0.18f},
        { P::oeqLowGain,   0.5f },
    })});
}

// =============================================================================
void PresetManager::applyPreset(const Preset& p)
{
    // 1. Reset every parameter to its default
    for (auto* param : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(param))
            rp->setValueNotifyingHost(rp->getDefaultValue());

    // 2. Apply overrides
    for (auto& [id, val] : p.values)
        if (auto* param = apvts.getParameter(id))
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(param))
                rp->setValueNotifyingHost(rp->convertTo0to1(val));
}

void PresetManager::loadPreset(int index)
{
    if (index < 0 || index >= numTotal()) return;

    if (index < numFactory())
        applyPreset(factory[index]);
    else
    {
        // User preset: restore full APVTS state from XML
        int ui = index - numFactory();
        if (ui >= numUser()) return;
        juce::XmlDocument doc(userDir().getChildFile(user[ui].name + ".xml"));
        if (auto xml = doc.getDocumentElement())
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }

    current = index;
    sendChangeMessage();
}

void PresetManager::saveUserPreset(const juce::String& name)
{
    if (name.isEmpty()) return;
    auto file = userDir().getChildFile(name + ".xml");
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        xml->writeToFile(file, {});

    refreshUserList();
    // Set current to the newly saved preset
    for (int i = numFactory(); i < numTotal(); ++i)
        if (currentName() == name || allNames()[i] == name)
            { current = i; break; }
    sendChangeMessage();
}

void PresetManager::deleteUserPreset(int userIndex)
{
    if (userIndex < 0 || userIndex >= numUser()) return;
    userDir().getChildFile(user[userIndex].name + ".xml").deleteFile();
    refreshUserList();
    current = std::clamp(current, 0, numTotal() - 1);
    sendChangeMessage();
}

void PresetManager::refreshUserList()
{
    user.clear();
    auto dir = userDir();
    for (auto& f : dir.findChildFiles(juce::File::findFiles, false, "*.xml"))
        user.push_back({ f.getFileNameWithoutExtension(), {} });
}

juce::File PresetManager::userDir() const
{
    auto d = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                 .getChildFile("TekkMojo")
                 .getChildFile("Presets");
    d.createDirectory();
    return d;
}

juce::StringArray PresetManager::allNames() const
{
    juce::StringArray names;
    for (auto& p : factory) names.add(p.name);
    for (auto& p : user)    names.add(p.name + "  *");
    return names;
}

juce::String PresetManager::currentName() const
{
    auto names = allNames();
    if (current >= 0 && current < names.size())
        return names[current];
    return "---";
}
