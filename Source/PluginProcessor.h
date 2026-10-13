#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Kick909Core.h"

class Kick909Processor : public juce::AudioProcessor
{
public:
    Kick909Processor();
    ~Kick909Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Kick 909"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyState;            // used by the on-screen TRIG button

    std::atomic<int>   currentPreset { 0 };      // last preset played / selected in the editor
    std::atomic<int>   lastNote { 36 };
    std::atomic<float> modWheel { 0.0f };        // CC1, 0..1
    std::atomic<float> pitchBend { 0.0f };       // -1..1

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    k909::Voice voice;
    float clipBase = 0.35f, clipEff = 0.35f, drive = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Kick909Processor)
};
