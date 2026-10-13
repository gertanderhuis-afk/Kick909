#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// Pure DSP for the 909-style bass drum. No JUCE dependency, so it can be tested standalone.
namespace k909
{
constexpr float kPi = 3.14159265358979f;
constexpr int   kNumPresets = 24;

// Preset mode listens to notes C6..B7 (24 notes). Middle C = C4 (MIDI 60) here, so C6 = 84.
// If your DAW calls MIDI 60 "C3" (Ableton, Logic), the same keys are C7..B8 on its keyboard;
// change this one number to move the preset range.
constexpr int   kPresetFirstNote = 84;
constexpr int   kChromaticRootNote = 36;   // in chromatic mode this note plays the Tune-knob pitch

struct Preset { const char* name; float tune, decay, attack, clip; };

inline const Preset kPresets[kNumPresets] = {
    { "CLASSIC",   0.45f, 0.50f, 0.50f, 0.35f }, { "TIGHT",     0.50f, 0.25f, 0.60f, 0.40f },
    { "DEEP",      0.15f, 0.70f, 0.35f, 0.30f }, { "LONG BOOM", 0.10f, 0.95f, 0.30f, 0.45f },
    { "PUNCH",     0.55f, 0.35f, 0.85f, 0.50f }, { "CLICK",     0.60f, 0.20f, 1.00f, 0.30f },
    { "SOFT",      0.35f, 0.45f, 0.10f, 0.10f }, { "FAT",       0.25f, 0.60f, 0.60f, 0.70f },
    { "THUMP",     0.30f, 0.40f, 0.20f, 0.55f }, { "HARD",      0.70f, 0.35f, 0.90f, 0.80f },
    { "DRY",       0.50f, 0.15f, 0.50f, 0.25f }, { "SUB",       0.00f, 0.85f, 0.15f, 0.20f },
    { "TECHNO",    0.40f, 0.55f, 0.75f, 0.60f }, { "HOUSE",     0.45f, 0.60f, 0.55f, 0.40f },
    { "GABBER",    0.55f, 0.90f, 0.70f, 1.00f }, { "ACID",      0.60f, 0.45f, 0.80f, 0.65f },
    { "TRANCE",    0.50f, 0.50f, 0.70f, 0.50f }, { "ELECTRO",   0.65f, 0.30f, 0.60f, 0.35f },
    { "TOM-ISH",   0.85f, 0.30f, 0.20f, 0.10f }, { "LOW DRIVE", 0.20f, 0.50f, 0.40f, 0.90f },
    { "CRUNCH",    0.40f, 0.40f, 0.65f, 0.95f }, { "SNAPPY",    0.75f, 0.12f, 0.95f, 0.45f },
    { "RUMBLE",    0.05f, 1.00f, 0.25f, 0.60f }, { "MAX",       0.50f, 0.70f, 1.00f, 1.00f },
};

// ---- parameter mappings -------------------------------------------------------------------
inline float tuneHz (float t01, bool x2)  { return 40.0f * std::exp2 (t01) * (x2 ? 2.0f : 1.0f); }
inline float decayMs (float d01, bool x2) { return 35.0f * std::pow (6.0f, d01) * (x2 ? 2.0f : 1.0f); }

inline float chromaticHz (int note, float t01, bool x2)
{
    return std::clamp (tuneHz (t01, x2) * std::exp2 ((float) (note - kChromaticRootNote) / 12.0f), 10.0f, 2000.0f);
}

// Velocity -> level before the clipper, so hard hits also clip harder (like an accented hit).
// These constants are a model, not a measurement of the hardware: tweak to taste.
inline float velocityCurve (float v01) { return 0.12f + 0.88f * std::pow (v01, 1.7f); }

inline float driveFromClip (float clip01) { return 1.0f + 24.0f * clip01 * clip01; }

// Antiparallel-diode style clipper: linear for small signals, logarithmic above the knee.
// Slight mismatch between the two diodes gives the even harmonics an analog clipper has.
inline float diodeClip (float x, float drive)
{
    const float d = x >= 0.0f ? drive : drive * 0.85f;
    return std::asinh (d * x) / std::asinh (drive);
}

// ---- 17-bit LFSR noise, clocked at 300 kHz -------------------------------------------------
// Each host sample averages however many 300 kHz clocks fall inside it (box filter = natural
// anti-aliasing), then is rescaled to unit variance.
struct Lfsr
{
    uint32_t state = 0x1ACE1u;
    double acc = 0.0, step = 1.0;
    float last = 0.0f;

    void prepare (double fs) { step = 300000.0 / fs; acc = 0.0; }

    float next()
    {
        acc += step;
        const int n = (int) acc;
        acc -= n;
        if (n <= 0) return last;
        int sum = 0;
        for (int i = 0; i < n; ++i)
        {
            const uint32_t bit = (state ^ (state >> 3)) & 1u;     // taps 17 and 14
            state = (state >> 1) | (bit << 16);
            sum += (state & 1u) ? 1 : -1;
        }
        return last = (float) sum / std::sqrt ((float) n);
    }
};

// ---- the voice -----------------------------------------------------------------------------
class Voice
{
public:
    void prepare (double fs)
    {
        sr = (float) fs;
        lfsr.prepare (fs);
        gHp = 1.0f - std::exp (-2.0f * kPi * 700.0f / sr);
        gLp = 1.0f - std::exp (-2.0f * kPi * 6500.0f / sr);
        noiseScale = std::sqrt (sr / 44100.0f);
        active = false;
    }

    void trigger (float baseHz, float tauMs, float attack01, float vel01)
    {
        f0 = baseHz;
        attack = attack01;
        velGain = velocityCurve (vel01);
        pitchAmt = 2.2f + 1.6f * attack01;
        phase = 0.0f; pitchEnv = 1.0f; clickEnv = 1.0f; amp = 1.0f; rise = 0.0f;
        decayCoef = std::exp (-1.0f / (tauMs * 0.001f * sr));
        pitchCoef = std::exp (-1.0f / (0.014f * sr));      // 14 ms pitch sweep
        clickCoef = std::exp (-1.0f / (0.0022f * sr));     // 2.2 ms click
        riseCoef  = std::exp (-1.0f / (0.00012f * sr));    // 0.12 ms turn-on (transistor VCA / RC)
        nLow = nBand = 0.0f;
        active = true;
    }

    void kill() { active = false; }
    bool isActive() const { return active; }

    float next (float bendRatio)
    {
        if (! active) return 0.0f;

        // oscillator with pitch envelope, starts at the waveform peak like a triggered VCO
        const float f = std::min (f0 * (1.0f + pitchAmt * pitchEnv) * bendRatio, 0.45f * sr);
        phase += f / sr;
        if (phase >= 1.0f) phase -= 1.0f;
        pitchEnv *= pitchCoef;

        float body = std::cos (2.0f * kPi * phase);
        body = std::tanh (1.25f * body) / std::tanh (1.25f);   // gentle transistor-VCA rounding

        // RC decay with a fast turn-on
        rise = 1.0f - (1.0f - rise) * riseCoef;
        amp *= decayCoef;
        const float env = amp * rise;

        // LFSR noise -> band-limited click
        const float n = lfsr.next() * noiseScale;
        nLow  += gHp * (n - nLow);
        const float hp = n - nLow;
        nBand += gLp * (hp - nBand);

        // phase-aware click: force the click's polarity to follow the body so they always add
        const float sgn = body >= 0.0f ? 1.0f : -1.0f;
        const float click = std::fabs (nBand) * sgn * clickEnv * attack * 0.55f;
        clickEnv *= clickCoef;

        if (amp < 1.0e-5f) active = false;
        return (body * env + click) * velGain;
    }

private:
    float sr = 44100.0f, f0 = 50.0f, attack = 0.5f, velGain = 1.0f, pitchAmt = 3.0f;
    float phase = 0.0f, pitchEnv = 0.0f, clickEnv = 0.0f, amp = 0.0f, rise = 0.0f;
    float decayCoef = 0.999f, pitchCoef = 0.99f, clickCoef = 0.9f, riseCoef = 0.5f;
    float gHp = 0.1f, gLp = 0.5f, nLow = 0.0f, nBand = 0.0f, noiseScale = 1.0f;
    bool active = false;
    Lfsr lfsr;
};
} // namespace k909
