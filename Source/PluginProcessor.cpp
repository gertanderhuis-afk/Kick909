#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

AudioProcessorValueTreeState::ParameterLayout Kick909Processor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto f = [&] (const char* id, const char* name, float def)
    {
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                      NormalisableRange<float> (0.0f, 1.0f, 0.001f), def));
    };
    f ("tune",   "Tune",   0.45f);
    f ("decay",  "Decay",  0.50f);
    f ("attack", "Attack", 0.50f);
    f ("clip",   "Clip",   0.35f);
    f ("level",  "Level",  0.80f);
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "tuneX2",    1 }, "Tune X2",    false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "decayX2",   1 }, "Decay X2",   false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "chromatic", 1 }, "Chromatic",  false));
    return l;
}

Kick909Processor::Kick909Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

void Kick909Processor::prepareToPlay (double sampleRate, int)
{
    voice.prepare (sampleRate);
}

bool Kick909Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == AudioChannelSet::stereo() || out == AudioChannelSet::mono();
}

void Kick909Processor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    keyState.processNextMidiBuffer (midi, 0, numSamples, true);

    const float tune = param ("tune"), decay = param ("decay"), attack = param ("attack");
    const float clip = param ("clip"), gain = param ("level");
    const bool tx2 = param ("tuneX2") > 0.5f, dx2 = param ("decayX2") > 0.5f;
    const bool chromatic = param ("chromatic") > 0.5f;

    if (chromatic) clipBase = clip;      // knob is live in chromatic mode

    auto updateDrive = [this]
    {
        const float mod = modWheel.load();
        clipEff = jlimit (0.0f, 1.0f, clipBase + mod * (1.0f - clipBase));   // CC1 pushes into the clipper
        drive = k909::driveFromClip (clipEff);
    };
    updateDrive();

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    int pos = 0;

    auto render = [&] (int end)
    {
        for (; pos < end; ++pos)
        {
            const float bendRatio = std::exp2 (pitchBend.load() );   // +-1 -> +-12 semitones = +-1 octave
            float s = voice.next (bendRatio);
            s = k909::diodeClip (s, drive) * gain;
            s = jlimit (-1.0f, 1.0f, s);
            left[pos] = s;
            if (right != nullptr) right[pos] = s;
        }
    };

    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();
        render (jlimit (0, numSamples, metadata.samplePosition));

        if (msg.isNoteOn())
        {
            const int n = msg.getNoteNumber();
            const float vel = msg.getFloatVelocity();

            if (chromatic)
            {
                voice.trigger (k909::chromaticHz (n, tune, tx2), k909::decayMs (decay, dx2), attack, vel);
                clipBase = clip;
                lastNote = n;
            }
            else
            {
                const int idx = n - k909::kPresetFirstNote;
                if (idx >= 0 && idx < k909::kNumPresets)
                {
                    const auto& p = k909::kPresets[idx];
                    voice.trigger (k909::tuneHz (p.tune, tx2), k909::decayMs (p.decay, dx2), p.attack, vel);
                    clipBase = p.clip;
                    currentPreset = idx;
                    lastNote = n;
                }
            }
            updateDrive();
        }
        else if (msg.isPitchWheel())
        {
            pitchBend = (float) (msg.getPitchWheelValue() - 8192) / 8192.0f;
        }
        else if (msg.isController() && msg.getControllerNumber() == 1)
        {
            modWheel = (float) msg.getControllerValue() / 127.0f;
            updateDrive();
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            voice.kill();
        }
    }
    render (numSamples);
}

AudioProcessorEditor* Kick909Processor::createEditor() { return new Kick909Editor (*this); }

void Kick909Processor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void Kick909Processor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Kick909Processor(); }
