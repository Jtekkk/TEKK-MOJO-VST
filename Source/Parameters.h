#pragma once
#include <JuceHeader.h>

namespace ParamIDs
{
    // Global
    static const juce::String inputTrim   = "INPUT_TRIM";
    static const juce::String outputTrim  = "OUTPUT_TRIM";
    static const juce::String oversample  = "OVERSAMPLE";
    static const juce::String autoGain    = "AUTO_GAIN";

    // Input Transformer
    static const juce::String inXfmrBypass = "IN_XFMR_BYPASS";
    static const juce::String inXfmrType   = "IN_XFMR_TYPE";
    static const juce::String inXfmrDrive  = "IN_XFMR_DRIVE";
    static const juce::String inXfmrTrim   = "IN_XFMR_TRIM";

    // High-Pass Filter
    static const juce::String hpfBypass = "HPF_BYPASS";
    static const juce::String hpfFreq   = "HPF_FREQ";
    static const juce::String hpfOrder  = "HPF_ORDER";

    // Induction EQ
    static const juce::String ieqBypass   = "IEQ_BYPASS";
    static const juce::String ieqLowFreq  = "IEQ_LOW_FREQ";
    static const juce::String ieqLowGain  = "IEQ_LOW_GAIN";
    static const juce::String ieqMidFreq  = "IEQ_MID_FREQ";
    static const juce::String ieqMidGain  = "IEQ_MID_GAIN";
    static const juce::String ieqMidQ     = "IEQ_MID_Q";
    static const juce::String ieqHighFreq = "IEQ_HIGH_FREQ";
    static const juce::String ieqHighGain = "IEQ_HIGH_GAIN";
    static const juce::String ieqSatDrive = "IEQ_SAT_DRIVE";

    // Compressor
    static const juce::String compBypass  = "COMP_BYPASS";
    static const juce::String compType    = "COMP_TYPE";
    static const juce::String compThresh  = "COMP_THRESH";
    static const juce::String compRatio   = "COMP_RATIO";
    static const juce::String compAttack  = "COMP_ATTACK";
    static const juce::String compRelease = "COMP_RELEASE";
    static const juce::String compMakeup  = "COMP_MAKEUP";
    static const juce::String compMix     = "COMP_MIX";

    // Tube + Tape Drive
    static const juce::String driveBypass = "DRIVE_BYPASS";
    static const juce::String driveMode   = "DRIVE_MODE";
    static const juce::String driveAmount = "DRIVE_AMOUNT";
    static const juce::String driveBlend  = "DRIVE_BLEND";
    static const juce::String driveTrim   = "DRIVE_TRIM";

    // Output EQ
    static const juce::String oeqBypass   = "OEQ_BYPASS";
    static const juce::String oeqLowFreq  = "OEQ_LOW_FREQ";
    static const juce::String oeqLowGain  = "OEQ_LOW_GAIN";
    static const juce::String oeqMidFreq  = "OEQ_MID_FREQ";
    static const juce::String oeqMidGain  = "OEQ_MID_GAIN";
    static const juce::String oeqMidQ     = "OEQ_MID_Q";
    static const juce::String oeqHighFreq = "OEQ_HIGH_FREQ";
    static const juce::String oeqHighGain = "OEQ_HIGH_GAIN";

    // Limiter
    static const juce::String limBypass  = "LIM_BYPASS";
    static const juce::String limThresh  = "LIM_THRESH";
    static const juce::String limRelease = "LIM_RELEASE";

    // Output Transformer
    static const juce::String outXfmrBypass = "OUT_XFMR_BYPASS";
    static const juce::String outXfmrType   = "OUT_XFMR_TYPE";
    static const juce::String outXfmrDrive  = "OUT_XFMR_DRIVE";
    static const juce::String outXfmrTrim   = "OUT_XFMR_TRIM";

    // Safety Clip
    static const juce::String clipBypass = "CLIP_BYPASS";
    static const juce::String clipLevel  = "CLIP_LEVEL";
}

namespace Params
{
    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        std::vector<std::unique_ptr<RangedAudioParameter>> p;

        // Global
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::inputTrim, "Input Trim",
            NormalisableRange<float>(-24.f, 24.f, 0.1f), 0.f,
            AudioParameterFloatAttributes().withLabel("dB")));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::outputTrim, "Output Trim",
            NormalisableRange<float>(-24.f, 24.f, 0.1f), 0.f,
            AudioParameterFloatAttributes().withLabel("dB")));
        p.push_back(std::make_unique<AudioParameterChoice>(
            ParamIDs::oversample, "Oversample",
            StringArray{"1x", "2x", "4x", "8x"}, 1));
        p.push_back(std::make_unique<AudioParameterBool>(
            ParamIDs::autoGain, "Auto-Gain", true));

        // Input Transformer
        p.push_back(std::make_unique<AudioParameterBool>(
            ParamIDs::inXfmrBypass, "In Xfmr Bypass", false));
        p.push_back(std::make_unique<AudioParameterChoice>(
            ParamIDs::inXfmrType, "In Xfmr Type",
            StringArray{"Iron I", "Iron II", "Iron III"}, 0));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::inXfmrDrive, "In Xfmr Drive",
            NormalisableRange<float>(0.f, 1.f, 0.01f), 0.3f));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::inXfmrTrim, "In Xfmr Trim",
            NormalisableRange<float>(-12.f, 12.f, 0.1f), 0.f,
            AudioParameterFloatAttributes().withLabel("dB")));

        // HPF
        p.push_back(std::make_unique<AudioParameterBool>(
            ParamIDs::hpfBypass, "HPF Bypass", false));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::hpfFreq, "HPF Freq",
            NormalisableRange<float>(20.f, 500.f, 1.f, 0.5f), 30.f,
            AudioParameterFloatAttributes().withLabel("Hz")));
        p.push_back(std::make_unique<AudioParameterChoice>(
            ParamIDs::hpfOrder, "HPF Order",
            StringArray{"6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct"}, 1));

        // Induction EQ
        p.push_back(std::make_unique<AudioParameterBool>(
            ParamIDs::ieqBypass, "Ind EQ Bypass", false));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::ieqLowFreq, "Ind Low Freq",
            NormalisableRange<float>(30.f, 500.f, 1.f, 0.5f), 100.f,
            AudioParameterFloatAttributes().withLabel("Hz")));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::ieqLowGain, "Ind Low Gain",
            NormalisableRange<float>(-12.f, 12.f, 0.1f), 0.f,
            AudioParameterFloatAttributes().withLabel("dB")));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::ieqMidFreq, "Ind Mid Freq",
            NormalisableRange<float>(200.f, 5000.f, 1.f, 0.5f), 1000.f,
            AudioParameterFloatAttributes().withLabel("Hz")));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::ieqMidGain, "Ind Mid Gain",
            NormalisableRange<float>(-12.f, 12.f, 0.1f), 0.f,
            AudioParameterFloatAttributes().withLabel("dB")));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::ieqMidQ, "Ind Mid Q",
            NormalisableRange<float>(0.3f, 4.f, 0.01f), 0.7f));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::ieqHighFreq, "Ind High Freq",
            NormalisableRange<float>(2000.f, 20000.f, 1.f, 0.5f), 8000.f,
            AudioParameterFloatAttributes().withLabel("Hz")));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::ieqHighGain, "Ind High Gain",
            NormalisableRange<float>(-12.f, 12.f, 0.1f), 0.f,
            AudioParameterFloatAttributes().withLabel("dB")));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::ieqSatDrive, "Ind Sat Drive",
            NormalisableRange<float>(0.f, 1.f, 0.01f), 0.2f));

        // Compressor
        p.push_back(std::make_unique<AudioParameterBool>(
            ParamIDs::compBypass, "Comp Bypass", false));
        p.push_back(std::make_unique<AudioParameterChoice>(
            ParamIDs::compType, "Comp Type",
            StringArray{"FET", "Opto", "VCA", "Vari-Mu"}, 0));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::compThresh, "Comp Thresh",
            NormalisableRange<float>(-40.f, 0.f, 0.1f), -12.f,
            AudioParameterFloatAttributes().withLabel("dB")));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::compRatio, "Comp Ratio",
            NormalisableRange<float>(1.f, 20.f, 0.1f, 0.5f), 4.f,
            AudioParameterFloatAttributes().withLabel(":1")));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::compAttack, "Comp Attack",
            NormalisableRange<float>(0.1f, 100.f, 0.1f, 0.5f), 10.f,
            AudioParameterFloatAttributes().withLabel("ms")));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::compRelease, "Comp Release",
            NormalisableRange<float>(10.f, 1000.f, 1.f, 0.5f), 100.f,
            AudioParameterFloatAttributes().withLabel("ms")));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::compMakeup, "Comp Makeup",
            NormalisableRange<float>(0.f, 24.f, 0.1f), 0.f,
            AudioParameterFloatAttributes().withLabel("dB")));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::compMix, "Comp Mix",
            NormalisableRange<float>(0.f, 1.f, 0.01f), 1.f));

        // Tube + Tape Drive
        p.push_back(std::make_unique<AudioParameterBool>(
            ParamIDs::driveBypass, "Drive Bypass", false));
        p.push_back(std::make_unique<AudioParameterChoice>(
            ParamIDs::driveMode, "Drive Mode",
            StringArray{"Tube", "Tape"}, 0));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::driveAmount, "Drive Amount",
            NormalisableRange<float>(0.f, 1.f, 0.01f), 0.3f));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::driveBlend, "Drive Blend",
            NormalisableRange<float>(0.f, 1.f, 0.01f), 1.f));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::driveTrim, "Drive Trim",
            NormalisableRange<float>(-12.f, 12.f, 0.1f), 0.f,
            AudioParameterFloatAttributes().withLabel("dB")));

        // Output EQ
        p.push_back(std::make_unique<AudioParameterBool>(
            ParamIDs::oeqBypass, "Out EQ Bypass", false));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::oeqLowFreq, "Out Low Freq",
            NormalisableRange<float>(30.f, 500.f, 1.f, 0.5f), 100.f,
            AudioParameterFloatAttributes().withLabel("Hz")));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::oeqLowGain, "Out Low Gain",
            NormalisableRange<float>(-12.f, 12.f, 0.1f), 0.f,
            AudioParameterFloatAttributes().withLabel("dB")));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::oeqMidFreq, "Out Mid Freq",
            NormalisableRange<float>(200.f, 5000.f, 1.f, 0.5f), 1000.f,
            AudioParameterFloatAttributes().withLabel("Hz")));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::oeqMidGain, "Out Mid Gain",
            NormalisableRange<float>(-12.f, 12.f, 0.1f), 0.f,
            AudioParameterFloatAttributes().withLabel("dB")));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::oeqMidQ, "Out Mid Q",
            NormalisableRange<float>(0.3f, 4.f, 0.01f), 0.7f));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::oeqHighFreq, "Out High Freq",
            NormalisableRange<float>(2000.f, 20000.f, 1.f, 0.5f), 8000.f,
            AudioParameterFloatAttributes().withLabel("Hz")));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::oeqHighGain, "Out High Gain",
            NormalisableRange<float>(-12.f, 12.f, 0.1f), 0.f,
            AudioParameterFloatAttributes().withLabel("dB")));

        // Limiter
        p.push_back(std::make_unique<AudioParameterBool>(
            ParamIDs::limBypass, "Limiter Bypass", false));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::limThresh, "Limiter Thresh",
            NormalisableRange<float>(-24.f, 0.f, 0.1f), -0.3f,
            AudioParameterFloatAttributes().withLabel("dBFS")));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::limRelease, "Limiter Release",
            NormalisableRange<float>(1.f, 500.f, 1.f, 0.5f), 50.f,
            AudioParameterFloatAttributes().withLabel("ms")));

        // Output Transformer
        p.push_back(std::make_unique<AudioParameterBool>(
            ParamIDs::outXfmrBypass, "Out Xfmr Bypass", false));
        p.push_back(std::make_unique<AudioParameterChoice>(
            ParamIDs::outXfmrType, "Out Xfmr Type",
            StringArray{"Iron I", "Iron II", "Iron III"}, 0));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::outXfmrDrive, "Out Xfmr Drive",
            NormalisableRange<float>(0.f, 1.f, 0.01f), 0.2f));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::outXfmrTrim, "Out Xfmr Trim",
            NormalisableRange<float>(-12.f, 12.f, 0.1f), 0.f,
            AudioParameterFloatAttributes().withLabel("dB")));

        // Safety Clip
        p.push_back(std::make_unique<AudioParameterBool>(
            ParamIDs::clipBypass, "Clip Bypass", false));
        p.push_back(std::make_unique<AudioParameterFloat>(
            ParamIDs::clipLevel, "Clip Level",
            NormalisableRange<float>(-6.f, 0.f, 0.1f), -0.1f,
            AudioParameterFloatAttributes().withLabel("dBFS")));

        return { p.begin(), p.end() };
    }
}
