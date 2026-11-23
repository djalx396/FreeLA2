#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace ui
{
    struct Finish { juce::Colour plate, ear, ink, rule, hi, slot; };
    const Finish& finish (int i);
    const juce::Colour bar { 0xff171614 }, barText { 0xffefe6d2 }, barSub { 0xffb9ae98 }, barBtn { 0xff2a2824 },
                       barWell { 0xff0c0b0a }, amber { 0xffe8a33d }, red { 0xffb3261e }, meterInk { 0xff2b241a };
    juce::Font cond (float size, bool bold = true, bool italic = false);
}

class FaceLook : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    juce::Font getSliderPopupFont (juce::Slider&) override { return ui::cond (15.f); }
};

// knob with the faceplate scale drawn around it
class FaceKnob : public juce::Component
{
public:
    FaceKnob (juce::AudioProcessorValueTreeState&, const juce::String& id, juce::StringArray nums, int ticks = 0, bool stepped = false);
    void paint (juce::Graphics&) override;
    void resized() override;
    void setInk (juce::Colour c) { ink = c; repaint(); }
    juce::Slider slider;
    int diameter = 84;
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    juce::StringArray nums; int nTicks; bool stepped; juce::Colour ink { 0xff26231f };
};

// toggle switch (bat lever) bound to a 2-choice parameter
class BatSwitch : public juce::Component
{
public:
    BatSwitch (juce::RangedAudioParameter& p);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment att;
};

// a row of buttons bound to a choice or bool parameter
class Segmented : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    Segmented (juce::RangedAudioParameter& p, juce::StringArray labels, juce::Array<int> values = {}, bool dark = true);
    std::function<bool (int)> isAvailable;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void setColours (juce::Colour ink, juce::Colour plate) { inkC = ink; plateC = plate; repaint(); }
private:
    void timerCallback() override { repaint(); }
    int current() const;
    juce::RangedAudioParameter& param; juce::StringArray labels; juce::Array<int> values; bool dark;
    juce::Colour inkC { 0xff26231f }, plateC { 0xffd6cfbf };
};

class VuMeter : public juce::Component
{
public:
    void setState (float needleVu, bool powered, bool outMode) { vu = needleVu; power = powered; out = outMode; repaint(); }
    void paint (juce::Graphics&) override;
private:
    float vu = 0; bool power = true, out = false;
};

class FreeLA2Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FreeLA2Editor (FreeLA2Processor&);
    ~FreeLA2Editor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    void timerCallback() override;
    juce::RangedAudioParameter& P (const juce::String& id) { return *proc.apvts.getParameter (id); }
    int choice (const juce::String& id) const { return juce::roundToInt (proc.apvts.getRawParameterValue (id)->load()); }

    FreeLA2Processor& proc;
    FaceLook look;
    FaceKnob preDrive, preOut, reduction, gain, ahead, emphasis, mix, clipDrive, clipOut;
    BatSwitch pushedSw, limitSw;
    Segmented filterSeg, osSeg, renderOsSeg, meterSeg, finishSeg;
    juce::TextButton renderBtn, phaseBtn;
    VuMeter vu;
    juce::Rectangle<int> powerArea;
    float vuSmooth = -20.f, outSmooth = 0.f; int clipHold = 0; int lastFinish = -1;
    juce::SharedResourcePointer<juce::TooltipWindow> tips;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreeLA2Editor)
};
