#include "PluginEditor.h"

using namespace juce;

static const Colour kPanel (0xffd9d5c9), kInk (0xff262626);
static const Colour kOrange (0xffe8571f), kRed (0xffcc2a1e), kYellow (0xfff0b323), kBlue (0xff6f8fa6);

NineLook::NineLook()
{
    setColour (TextButton::buttonColourId, Colour (0xff3a3a3a));
    setColour (TextButton::buttonOnColourId, Colour (0xff555555));
    setColour (TextButton::textColourOffId, Colours::white);
    setColour (TextButton::textColourOnId, Colours::white);
    setColour (Slider::thumbColourId, Colours::white);
    setColour (TooltipWindow::backgroundColourId, Colour (0xff1a1a1a));
    setColour (TooltipWindow::textColourId, Colour (0xffff3b2a));
}

void NineLook::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                 float startAngle, float endAngle, Slider&)
{
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();

    g.setColour (kInk);
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / 10.0f;
        const bool major = (i % 5 == 0);
        const float r0 = r * (major ? 0.80f : 0.88f);
        g.drawLine (c.x + std::sin (a) * r0, c.y - std::cos (a) * r0,
                    c.x + std::sin (a) * r,  c.y - std::cos (a) * r, major ? 2.2f : 1.3f);
    }

    const float kr = r * 0.66f;
    g.setColour (Colours::black.withAlpha (0.35f));
    g.fillEllipse (c.x - kr, c.y - kr + 3.0f, kr * 2.0f, kr * 2.0f);

    ColourGradient grad (Colour (0xff666666), c.x - kr * 0.5f, c.y - kr * 0.7f,
                         Colour (0xff121212), c.x + kr * 0.7f, c.y + kr, true);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (Colour (0xff0a0a0a));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 1.5f);

    const float a = startAngle + (endAngle - startAngle) * pos;
    g.setColour (Colours::white);
    g.drawLine (c.x + std::sin (a) * kr * 0.25f, c.y - std::cos (a) * kr * 0.25f,
                c.x + std::sin (a) * kr * 0.92f, c.y - std::cos (a) * kr * 0.92f, 3.0f);
}

void NineLook::drawToggleButton (Graphics& g, ToggleButton& bt, bool highlighted, bool)
{
    auto r = bt.getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (highlighted ? Colour (0xff4a4a4a) : Colour (0xff383838));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Colours::black);
    g.drawRoundedRectangle (r, 4.0f, 1.2f);

    auto led = Rectangle<float> (r.getCentreX() - 14.0f, r.getY() + 8.0f, 28.0f, 7.0f);
    const bool on = bt.getToggleState();
    if (on) { g.setColour (Colour (0xffff3b2a).withAlpha (0.35f)); g.fillRoundedRectangle (led.expanded (3.0f), 4.0f); }
    g.setColour (on ? Colour (0xffff4a38) : Colour (0xff4b1612));
    g.fillRoundedRectangle (led, 2.0f);

    g.setColour (Colours::white);
    g.setFont (font (13.0f));
    g.drawText (bt.getButtonText(), r.withTrimmedTop (20.0f).toNearestInt(), Justification::centred);
}

Kick909Editor::Kick909Editor (Kick909Processor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);
    setSize (720, 372);

    Slider* all[5] = { &tune, &decay, &attack, &clip, &level };
    for (int i = 0; i < 5; ++i)
    {
        knobs[i] = all[i];
        all[i]->setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        all[i]->setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
        all[i]->setRotaryParameters (degreesToRadians (225.0f), degreesToRadians (495.0f), true);
        all[i]->setPopupDisplayEnabled (true, true, this);
        all[i]->setDoubleClickReturnValue (true, i == 0 ? 0.45 : i == 1 ? 0.5 : i == 2 ? 0.5 : i == 3 ? 0.35 : 0.8);
        addAndMakeVisible (*all[i]);
    }
    tuneA = std::make_unique<SA> (proc.apvts, "tune", tune);
    decayA = std::make_unique<SA> (proc.apvts, "decay", decay);
    attackA = std::make_unique<SA> (proc.apvts, "attack", attack);
    clipA = std::make_unique<SA> (proc.apvts, "clip", clip);
    levelA = std::make_unique<SA> (proc.apvts, "level", level);

    for (auto* b : { &chromatic, &tuneX2, &decayX2 }) addAndMakeVisible (*b);
    chromaticA = std::make_unique<BA> (proc.apvts, "chromatic", chromatic);
    tuneX2A = std::make_unique<BA> (proc.apvts, "tuneX2", tuneX2);
    decayX2A = std::make_unique<BA> (proc.apvts, "decayX2", decayX2);

    for (auto* b : { &trig, &prev, &next }) addAndMakeVisible (*b);

    trig.onStateChange = [this]
    {
        if (trig.isDown() && ! trigHeld)
        {
            trigHeld = true;
            heldNote = auditionNote();
            proc.keyState.noteOn (1, heldNote, 0.8f);
        }
        else if (! trig.isDown() && trigHeld)
        {
            trigHeld = false;
            proc.keyState.noteOff (1, heldNote, 0.0f);
        }
    };
    auto step = [this] (int d)
    {
        if (proc.apvts.getRawParameterValue ("chromatic")->load() > 0.5f) return;
        const int n = k909::kNumPresets;
        proc.currentPreset = ((proc.currentPreset.load() + d) % n + n) % n;
        const int note = auditionNote();
        proc.keyState.noteOn (1, note, 0.8f);
        proc.keyState.noteOff (1, note, 0.0f);
    };
    prev.onClick = [step] { step (-1); };
    next.onClick = [step] { step (1); };

    startTimerHz (30);
}

Kick909Editor::~Kick909Editor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

int Kick909Editor::auditionNote() const
{
    return proc.apvts.getRawParameterValue ("chromatic")->load() > 0.5f
               ? k909::kChromaticRootNote
               : k909::kPresetFirstNote + proc.currentPreset.load();
}

void Kick909Editor::timerCallback()
{
    const bool chrom = proc.apvts.getRawParameterValue ("chromatic")->load() > 0.5f;
    for (int i = 0; i < 4; ++i) knobs[i]->setAlpha (chrom ? 1.0f : 0.4f);   // knobs are ignored in preset mode
    repaint (displayArea);
}

void Kick909Editor::paint (Graphics& g)
{
    const int W = getWidth();
    g.fillAll (kPanel);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.25f), 0, 0, Colours::black.withAlpha (0.10f), 0, (float) getHeight(), false));
    g.fillAll();

    g.setColour (kInk);
    g.setFont (NineLook::font (32.0f));
    g.drawText ("KICK 909", 28, 10, 300, 36, Justification::centredLeft);
    g.setFont (NineLook::font (12.0f, false));
    g.drawText ("ELECTRONIC BASS DRUM", 30, 42, 300, 16, Justification::centredLeft);

    // colour stripes
    const Colour stripes[4] = { kOrange, kRed, kYellow, kBlue };
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (stripes[i]);
        g.fillRect (0, 66 + i * 5, W, 5);
    }

    g.setColour (kInk);
    g.setFont (NineLook::font (13.0f));
    for (int i = 0; i < 5; ++i)
        g.drawText (knobNames[i], knobs[i]->getBounds().getX(), 214, knobs[i]->getWidth(), 18, Justification::centred);

    // LED display
    const bool chrom = proc.apvts.getRawParameterValue ("chromatic")->load() > 0.5f;
    g.setColour (Colour (0xff160807));
    g.fillRoundedRectangle (displayArea.toFloat(), 5.0f);
    g.setColour (Colours::black);
    g.drawRoundedRectangle (displayArea.toFloat(), 5.0f, 1.5f);

    const auto led = Colour (0xffff3b2a);
    String line1, line2;
    if (chrom)
    {
        line1 = "CHROMATIC";
        line2 = "NOTE " + MidiMessage::getMidiNoteName (proc.lastNote.load(), true, true, 3);
    }
    else
    {
        const int p = proc.currentPreset.load();
        line1 = String (p + 1).paddedLeft ('0', 2) + " " + k909::kPresets[p].name;
        line2 = "PRESET C6-B7";
    }
    g.setColour (led);
    g.setFont (Font (FontOptions (Font::getDefaultMonospacedFontName(), 24.0f, Font::bold)));
    g.drawText (line1, displayArea.withTrimmedBottom (26).reduced (10, 0), Justification::centredLeft);
    g.setColour (led.withAlpha (0.7f));
    g.setFont (Font (FontOptions (Font::getDefaultMonospacedFontName(), 12.0f, Font::plain)));
    g.drawText (line2 + "   MOD " + String (proc.modWheel.load(), 2)
                      + "   BEND " + String (proc.pitchBend.load() * 12.0f, 1),
                displayArea.withTrimmedTop (displayArea.getHeight() - 28).reduced (10, 0), Justification::centredLeft);

    g.setColour (kInk.withAlpha (0.65f));
    g.setFont (NineLook::font (11.0f, false));
    g.drawText ("CC1 MOD WHEEL = CLIP    PITCH BEND = +/-12 SEMITONES    KNOBS ACTIVE IN CHROMATIC MODE",
                0, getHeight() - 24, W, 16, Justification::centred);
}

void Kick909Editor::resized()
{
    for (int i = 0; i < 5; ++i)
        knobs[i]->setBounds (30 + i * 135, 96, 120, 112);

    displayArea = { 30, 250, 250, 76 };
    prev.setBounds (292, 250, 40, 34);
    next.setBounds (336, 250, 40, 34);
    trig.setBounds (292, 292, 84, 34);

    chromatic.setBounds (400, 250, 92, 76);
    tuneX2.setBounds (504, 250, 92, 76);
    decayX2.setBounds (608, 250, 92, 76);
}
