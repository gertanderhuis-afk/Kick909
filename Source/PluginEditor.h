#pragma once
#include "PluginProcessor.h"

// Hardware-panel look: cream-grey panel, dark skirted knobs with scale ticks, LED buttons.
class NineLook : public juce::LookAndFeel_V4
{
public:
    NineLook();
    static juce::Font font (float h, bool bold = true)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (14.0f); }
};

class Kick909Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Kick909Editor (Kick909Processor&);
    ~Kick909Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    int  auditionNote() const;

    Kick909Processor& proc;
    NineLook look;

    juce::Slider tune, decay, attack, clip, level;
    juce::ToggleButton chromatic { "CHROMATIC" }, tuneX2 { "TUNE X2" }, decayX2 { "DECAY X2" };
    juce::TextButton trig { "TRIG" }, prev { "<" }, next { ">" };

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SA> tuneA, decayA, attackA, clipA, levelA;
    std::unique_ptr<BA> chromaticA, tuneX2A, decayX2A;

    juce::Rectangle<int> displayArea;
    juce::Slider* knobs[5] {};
    const char* knobNames[5] = { "TUNE", "DECAY", "ATTACK", "CLIP", "LEVEL" };
    bool trigHeld = false;
    int heldNote = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Kick909Editor)
};
