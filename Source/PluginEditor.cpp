#include "PluginEditor.h"
#include "Parameters.h"

using namespace ParamIDs;
namespace C = MojoColors;

TekkMojoEditor::TekkMojoEditor(TekkMojoProcessor& p)
    : AudioProcessorEditor(p), proc(p),
      osCombo    ("Oversample", p.apvts, oversample),
      inTrimKnob ("In Trim",   p.apvts, inputTrim),
      outTrimKnob("Out Trim",  p.apvts, outputTrim),
      panelInXfmr ("Input Transformer", C::saturation, p.apvts, inXfmrBypass),
      panelHPF    ("High-Pass Filter",  C::toneDyn,    p.apvts, hpfBypass),
      panelIEQ    ("Induction EQ",      C::toneDyn,    p.apvts, ieqBypass),
      panelComp   ("Compressor",        C::toneDyn,    p.apvts, compBypass),
      panelDrive  ("Tube + Tape Drive", C::saturation, p.apvts, driveBypass),
      panelOEQ    ("Output EQ",         C::toneDyn,    p.apvts, oeqBypass),
      panelLim    ("Limiter",           C::toneDyn,    p.apvts, limBypass),
      panelOutXfmr("Output Transformer",C::saturation, p.apvts, outXfmrBypass),
      panelClip   ("Safety Clip",       C::utility,    p.apvts, clipBypass)
{
    setLookAndFeel(&laf);
    buildStages();

    // AGC toggle
    agcButton.setButtonText("AGC");
    agcButton.setToggleable(true);
    agcAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        p.apvts, autoGain, agcButton);
    addAndMakeVisible(agcButton);

    // Title
    pluginTitle.setText("TEKK MOJO", juce::dontSendNotification);
    pluginTitle.setFont(juce::Font(22.f, juce::Font::bold));
    pluginTitle.setColour(juce::Label::textColourId, C::knobFill);
    pluginTitle.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(pluginTitle);

    addAndMakeVisible(osCombo);
    addAndMakeVisible(inTrimKnob);
    addAndMakeVisible(outTrimKnob);

    // Level meters
    inMeter.setSource(&proc.inputMeter());
    outMeter.setSource(&proc.outputMeter());
    addAndMakeVisible(inMeter);
    addAndMakeVisible(outMeter);

    // ---- Preset bar ----
    syncPresetCombo();
    presetCombo.onChange = [this]
    {
        int idx = presetCombo.getSelectedItemIndex();
        if (idx >= 0)
            proc.presets.loadPreset(idx);
    };
    addAndMakeVisible(presetCombo);

    prevBtn.onClick = [this]
    {
        int next = std::max(0, proc.presets.currentIndex() - 1);
        proc.presets.loadPreset(next);
        syncPresetCombo();
    };
    addAndMakeVisible(prevBtn);

    nextBtn.onClick = [this]
    {
        int next = std::min(proc.presets.numTotal() - 1,
                            proc.presets.currentIndex() + 1);
        proc.presets.loadPreset(next);
        syncPresetCombo();
    };
    addAndMakeVisible(nextBtn);

    saveBtn.onClick = [this] { promptSavePreset(); };
    addAndMakeVisible(saveBtn);

    // Keep combo in sync when presets are loaded externally (e.g. DAW recall)
    proc.presets.addChangeListener(this);

    // A/B comparison
    refreshABButtons();
    abBtnA.onClick = [this] {
        proc.abState.select(0);
        refreshABButtons();
        syncPresetCombo();
    };
    abBtnB.onClick = [this] {
        proc.abState.select(1);
        refreshABButtons();
        syncPresetCombo();
    };
    abCopy.onClick = [this] {
        proc.abState.copyActiveToOther();
        // no visual change — just overwrites the inactive snapshot
    };
    abCopy.setTooltip("Copy current state to the other slot");
    addAndMakeVisible(abBtnA);
    addAndMakeVisible(abBtnB);
    addAndMakeVisible(abCopy);

    // Stages viewport
    stagesContainer.addAndMakeVisible(panelInXfmr);
    stagesContainer.addAndMakeVisible(panelHPF);
    stagesContainer.addAndMakeVisible(panelIEQ);
    stagesContainer.addAndMakeVisible(panelComp);
    stagesContainer.addAndMakeVisible(panelDrive);
    stagesContainer.addAndMakeVisible(panelOEQ);
    stagesContainer.addAndMakeVisible(panelLim);
    stagesContainer.addAndMakeVisible(panelOutXfmr);
    stagesContainer.addAndMakeVisible(panelClip);

    viewport.setViewedComponent(&stagesContainer, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);

    setSize(560, 750);
    setResizable(true, true);
    setResizeLimits(480, 620, 900, 1200);
}

TekkMojoEditor::~TekkMojoEditor()
{
    proc.presets.removeAllChangeListeners();
    setLookAndFeel(nullptr);
}

void TekkMojoEditor::syncPresetCombo()
{
    presetCombo.clear(juce::dontSendNotification);
    int id = 1;

    // Factory group
    presetCombo.addSectionHeading("Factory");
    for (int i = 0; i < proc.presets.numFactory(); ++i)
        presetCombo.addItem(proc.presets.allNames()[i], id++);

    // User group
    if (proc.presets.numUser() > 0)
    {
        presetCombo.addSeparator();
        presetCombo.addSectionHeading("User");
        for (int i = proc.presets.numFactory(); i < proc.presets.numTotal(); ++i)
            presetCombo.addItem(proc.presets.allNames()[i], id++);
    }

    presetCombo.setSelectedItemIndex(proc.presets.currentIndex(),
                                     juce::dontSendNotification);
}

void TekkMojoEditor::promptSavePreset()
{
    auto* window = new juce::AlertWindow("Save Preset",
                                         "Enter a name for this preset:",
                                         juce::MessageBoxIconType::NoIcon);
    window->addTextEditor("name", proc.presets.currentName(), "Name:");
    window->addButton("Save",   1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    // SafePointer guards against the editor being destroyed before the dialog closes
    juce::Component::SafePointer<TekkMojoEditor> safeThis(this);
    window->enterModalState(true, juce::ModalCallbackFunction::create(
        [safeThis, window](int result)
        {
            if (result == 1)
            {
                auto name = window->getTextEditorContents("name").trim();
                if (name.isNotEmpty() && safeThis != nullptr)
                {
                    safeThis->proc.presets.saveUserPreset(name);
                    safeThis->syncPresetCombo();
                }
            }
            delete window;
        }));
}

void TekkMojoEditor::buildStages()
{
    auto& a = proc.apvts;

    panelInXfmr
        .addCombo("Iron",  a, inXfmrType)
        .addKnob ("Drive", a, inXfmrDrive)
        .addKnob ("Trim",  a, inXfmrTrim);

    panelHPF
        .addKnob ("Freq",  a, hpfFreq)
        .addCombo("Order", a, hpfOrder);

    panelIEQ
        .addKnob("Lo Freq",  a, ieqLowFreq)  .addKnob("Lo Gain", a, ieqLowGain)
        .addKnob("Mid Freq", a, ieqMidFreq)  .addKnob("Mid Gain",a, ieqMidGain)
        .addKnob("Mid Q",    a, ieqMidQ)
        .addKnob("Hi Freq",  a, ieqHighFreq) .addKnob("Hi Gain", a, ieqHighGain)
        .addKnob("Sat",      a, ieqSatDrive);

    panelComp
        .addCombo("Type",    a, compType)
        .addKnob ("Thresh",  a, compThresh)
        .addKnob ("Ratio",   a, compRatio)
        .addKnob ("Attack",  a, compAttack)
        .addKnob ("Release", a, compRelease)
        .addKnob ("Makeup",  a, compMakeup)
        .addKnob ("Mix",     a, compMix);
    panelComp.setGRSource(&proc.compGRdB());

    panelDrive
        .addCombo("Mode",  a, driveMode)
        .addKnob ("Drive", a, driveAmount)
        .addKnob ("Blend", a, driveBlend)
        .addKnob ("Trim",  a, driveTrim);

    panelOEQ
        .addKnob("Lo Freq",  a, oeqLowFreq)  .addKnob("Lo Gain", a, oeqLowGain)
        .addKnob("Mid Freq", a, oeqMidFreq)  .addKnob("Mid Gain",a, oeqMidGain)
        .addKnob("Mid Q",    a, oeqMidQ)
        .addKnob("Hi Freq",  a, oeqHighFreq) .addKnob("Hi Gain", a, oeqHighGain);

    panelLim
        .addKnob("Thresh",  a, limThresh)
        .addKnob("Release", a, limRelease);

    panelOutXfmr
        .addCombo("Iron",  a, outXfmrType)
        .addKnob ("Drive", a, outXfmrDrive)
        .addKnob ("Trim",  a, outXfmrTrim);

    panelClip
        .addKnob("Ceiling", a, clipLevel);
}

void TekkMojoEditor::refreshABButtons()
{
    int active = proc.abState.getActive();
    auto style = [](juce::TextButton& btn, bool on) {
        btn.setColour(juce::TextButton::buttonColourId,
                      on ? MojoColors::knobFill : MojoColors::panelBg);
        btn.setColour(juce::TextButton::buttonOnColourId,
                      on ? MojoColors::knobFill : MojoColors::panelBg);
        btn.setColour(juce::TextButton::textColourOffId,
                      on ? MojoColors::background : MojoColors::textDim);
        btn.setColour(juce::TextButton::textColourOnId,
                      on ? MojoColors::background : MojoColors::textDim);
    };
    style(abBtnA, active == 0);
    style(abBtnB, active == 1);
    abCopy.setTooltip(active == 0 ? "Copy A \xe2\x86\x92 B" : "Copy B \xe2\x86\x92 A");
}

void TekkMojoEditor::paint(juce::Graphics& g)
{
    g.fillAll(C::background);
    juce::ColourGradient grad(C::saturation.withAlpha(0.18f), 0, 0,
                              juce::Colours::transparentBlack, 0, 85, false);
    g.setGradientFill(grad);
    g.fillRect(0, 0, getWidth(), 85);

    // Subtle separator below preset bar
    g.setColour(MojoColors::panelBorder);
    g.fillRect(8, 83, getWidth() - 16, 1);
}

void TekkMojoEditor::resized()
{
    auto b = getLocalBounds().reduced(8);

    // Header row
    auto header = b.removeFromTop(52);
    pluginTitle .setBounds(header.removeFromLeft(120));
    inMeter     .setBounds(header.removeFromLeft(26).reduced(2, 8));
    outMeter    .setBounds(header.removeFromLeft(26).reduced(2, 8));
    outTrimKnob .setBounds(header.removeFromRight(58).reduced(0, 4));
    inTrimKnob  .setBounds(header.removeFromRight(58).reduced(0, 4));
    agcButton   .setBounds(header.removeFromRight(44).reduced(4, 12));
    osCombo     .setBounds(header.removeFromRight(90).reduced(4, 12));
    header.removeFromRight(8);
    abBtnB      .setBounds(header.removeFromRight(28).reduced(1, 12));
    abCopy      .setBounds(header.removeFromRight(22).reduced(1, 12));
    abBtnA      .setBounds(header.removeFromRight(28).reduced(1, 12));

    // Preset bar
    auto presetRow = b.removeFromTop(28);
    prevBtn    .setBounds(presetRow.removeFromLeft(26).reduced(1, 3));
    nextBtn    .setBounds(presetRow.removeFromLeft(26).reduced(1, 3));
    presetCombo.setBounds(presetRow.removeFromLeft(presetRow.getWidth() - 58).reduced(2, 3));
    saveBtn    .setBounds(presetRow.reduced(2, 3));

    b.removeFromTop(4);

    // Stage panels in viewport
    constexpr int gap  = 5;
    constexpr int panH = StagePanel::kTotalH;
    constexpr int nPanels = 9;
    int containerH = nPanels * panH + (nPanels - 1) * gap;

    viewport.setBounds(b);
    stagesContainer.setBounds(0, 0, b.getWidth() - 8, containerH);

    int pw = stagesContainer.getWidth(), y = 0;
    for (auto* panel : { &panelInXfmr, &panelHPF,    &panelIEQ,
                         &panelComp,   &panelDrive,   &panelOEQ,
                         &panelLim,    &panelOutXfmr, &panelClip })
    {
        panel->setBounds(0, y, pw, panH);
        y += panH + gap;
    }
}
