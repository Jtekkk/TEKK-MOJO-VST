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
    pluginTitle.setFont(juce::Font(30.f, juce::Font::bold));
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
    viewport.setScrollBarThickness(12);
    addAndMakeVisible(viewport);

    setSize(1000, 860);
    setResizable(true, true);
    setResizeLimits(820, 640, 1500, 1500);
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

    window->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, window](int result)
        {
            if (result == 1)
            {
                auto name = window->getTextEditorContents("name").trim();
                if (name.isNotEmpty())
                {
                    proc.presets.saveUserPreset(name);
                    syncPresetCombo();
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
                              juce::Colours::transparentBlack, 0, 124, false);
    g.setGradientFill(grad);
    g.fillRect(0, 0, getWidth(), 124);

    // Subtle separator below preset bar
    g.setColour(MojoColors::panelBorder);
    g.fillRect(8, 124, getWidth() - 16, 1);
}

void TekkMojoEditor::resized()
{
    auto b = getLocalBounds().reduced(8);

    // Header row
    auto header = b.removeFromTop(76);
    pluginTitle .setBounds(header.removeFromLeft(210));
    inMeter     .setBounds(header.removeFromLeft(34).reduced(3, 10));
    outMeter    .setBounds(header.removeFromLeft(34).reduced(3, 10));
    outTrimKnob .setBounds(header.removeFromRight(76).reduced(3, 6));
    inTrimKnob  .setBounds(header.removeFromRight(76).reduced(3, 6));
    agcButton   .setBounds(header.removeFromRight(60).reduced(6, 22));
    osCombo     .setBounds(header.removeFromRight(124).reduced(6, 22));
    header.removeFromRight(12);
    abBtnB      .setBounds(header.removeFromRight(38).reduced(2, 22));
    abCopy      .setBounds(header.removeFromRight(32).reduced(2, 22));
    abBtnA      .setBounds(header.removeFromRight(38).reduced(2, 22));

    // Preset bar
    auto presetRow = b.removeFromTop(40);
    prevBtn    .setBounds(presetRow.removeFromLeft(34).reduced(2, 4));
    nextBtn    .setBounds(presetRow.removeFromLeft(34).reduced(2, 4));
    saveBtn    .setBounds(presetRow.removeFromRight(88).reduced(2, 4));
    presetCombo.setBounds(presetRow.reduced(4, 4));

    b.removeFromTop(6);

    // Stage panels in viewport
    constexpr int gap  = 8;
    constexpr int panH = StagePanel::kTotalH;
    constexpr int nPanels = 9;
    int containerH = nPanels * panH + (nPanels - 1) * gap;

    viewport.setBounds(b);
    int innerW = b.getWidth() - viewport.getScrollBarThickness() - 4;
    stagesContainer.setBounds(0, 0, innerW, containerH);

    int pw = stagesContainer.getWidth(), y = 0;
    for (auto* panel : { &panelInXfmr, &panelHPF,    &panelIEQ,
                         &panelComp,   &panelDrive,   &panelOEQ,
                         &panelLim,    &panelOutXfmr, &panelClip })
    {
        panel->setBounds(0, y, pw, panH);
        y += panH + gap;
    }
}
