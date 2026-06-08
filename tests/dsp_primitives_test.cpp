// Standalone test for DSP.h — no JUCE dependency.
// Compile: g++ -std=c++17 -I../Source -o dsp_primitives_test dsp_primitives_test.cpp
#include "dsp/DSP.h"
#include <cmath>
#include <cstdio>
#include <cstring>

static int failures = 0;
static int passes   = 0;

static void check(bool cond, const char* msg)
{
    if (cond) { printf("PASS  %s\n", msg); ++passes; }
    else      { printf("FAIL  %s\n", msg); ++failures; }
}

// ---- Saturation shapes -------------------------------------------------------

static void test_tanhSat()
{
    // Odd symmetry
    check(std::abs(DSP::tanhSat(0.5f, 0.3f) + DSP::tanhSat(-0.5f, 0.3f)) < 1e-6f,
          "tanhSat: odd symmetry");
    // Bounded to (-1, 1)
    check(std::abs(DSP::tanhSat(1e6f, 1.f)) <= 1.f,  "tanhSat: bounded +");
    check(std::abs(DSP::tanhSat(-1e6f, 1.f)) <= 1.f, "tanhSat: bounded -");
    // Zero input -> zero output
    check(DSP::tanhSat(0.f, 0.5f) == 0.f, "tanhSat: zero in -> zero out");
    // At very large x, output is bounded to 1 regardless of drive
    check(std::abs(DSP::tanhSat(100.f, 0.f) - 1.f) < 0.001f, "tanhSat: large x saturates to 1");
    // Gain (output/input) decreases as input grows — classic saturation curve
    float gainSmall = DSP::tanhSat(0.001f, 0.5f) / 0.001f; // near-linear region
    float gainLarge = DSP::tanhSat(5.f, 0.5f)   / 5.f;     // saturated region
    check(gainSmall > gainLarge, "tanhSat: gain compresses at large amplitude");
}

static void test_tubeSat()
{
    // Zero input -> zero output
    check(std::abs(DSP::tubeSat(0.f, 0.f)) < 1e-6f, "tubeSat: zero in -> zero out");
    // Asymmetry: tubeSat(x) != -tubeSat(-x) due to x^2 term
    float pos = DSP::tubeSat(0.5f, 0.5f);
    float neg = DSP::tubeSat(-0.5f, 0.5f);
    check(std::abs(pos + neg) > 1e-4f, "tubeSat: asymmetric (even harmonics)");
    // Positive input produces positive output
    check(DSP::tubeSat(0.3f, 0.3f) > 0.f, "tubeSat: positive in -> positive out");
}

static void test_tapeSat()
{
    // Zero -> zero
    check(std::abs(DSP::tapeSat(0.f, 0.5f)) < 1e-6f, "tapeSat: zero in -> zero out");
    // Odd symmetry
    float a = DSP::tapeSat(0.5f, 0.4f);
    float b = DSP::tapeSat(-0.5f, 0.4f);
    check(std::abs(a + b) < 1e-5f, "tapeSat: odd symmetry");
    // Bounded: sign * (1 - exp(-|x*g|)) <= 1
    check(std::abs(DSP::tapeSat(100.f, 1.f)) <= 1.f, "tapeSat: bounded");
}

static void test_hardClip()
{
    check(DSP::hardClip(2.f,  1.f) ==  1.f, "hardClip: clips positive");
    check(DSP::hardClip(-2.f, 1.f) == -1.f, "hardClip: clips negative");
    check(std::abs(DSP::hardClip(0.5f, 1.f) - 0.5f) < 1e-6f, "hardClip: pass-through");
    check(DSP::hardClip(0.f, 1.f) == 0.f, "hardClip: zero");
}

// ---- OnePole ----------------------------------------------------------------

static void test_onepole()
{
    DSP::OnePole lp;
    lp.setFreq(1.f, 44100.f); // 1 Hz LP — very slow follower
    // DC convergence: after many samples, output -> 1
    for (int i = 0; i < 200000; ++i) lp.processLP(1.f);
    check(std::abs(lp.processLP(1.f) - 1.f) < 0.001f, "OnePole: DC converges to input");

    // Zero input -> zero output (fresh filter)
    DSP::OnePole lp2;
    lp2.setFreq(100.f, 44100.f);
    check(lp2.processLP(0.f) == 0.f, "OnePole: zero state, zero input -> zero out");
}

// ---- Biquad -----------------------------------------------------------------

static void test_biquad()
{
    DSP::Biquad bq;

    // PeakEQ at 0 dB gain = allpass (unity at all freqs)
    bq.setPeakEQ(1000.0, 44100.0, 0.0, 0.7);
    bq.reset();
    for (int i = 0; i < 200; ++i) bq.process(0.5f);
    float out = bq.process(0.5f);
    check(std::abs(out - 0.5f) < 1e-4f, "Biquad PeakEQ 0 dB: unity gain");

    // LowShelf at 0 dB = unity
    bq.setLowShelf(100.0, 44100.0, 0.0);
    bq.reset();
    for (int i = 0; i < 200; ++i) bq.process(0.5f);
    out = bq.process(0.5f);
    check(std::abs(out - 0.5f) < 1e-4f, "Biquad LowShelf 0 dB: unity gain");

    // HighShelf at 0 dB = unity
    bq.setHighShelf(8000.0, 44100.0, 0.0);
    bq.reset();
    for (int i = 0; i < 200; ++i) bq.process(0.5f);
    out = bq.process(0.5f);
    check(std::abs(out - 0.5f) < 1e-4f, "Biquad HighShelf 0 dB: unity gain");

    // HPF attenuates DC
    bq.setHighPass(100.0, 44100.0, 0.7071);
    bq.reset();
    for (int i = 0; i < 50000; ++i) bq.process(1.f);
    float dcOut = bq.process(1.f);
    check(std::abs(dcOut) < 0.01f, "Biquad HPF: DC -> near zero");

    // HPF passes high-frequency content well above cutoff
    // Use a 10 kHz sine through a 30 Hz HPF — should pass nearly unattenuated
    bq.setHighPass(30.0, 44100.0, 0.7071);
    bq.reset();
    float maxHF = 0.f;
    for (int i = 0; i < 2000; ++i) {
        float s = std::sin(2.f * 3.14159f * 10000.f * i / 44100.f);
        float y = bq.process(s);
        if (i > 200) maxHF = std::max(maxHF, std::abs(y));
    }
    check(maxHF > 0.9f, "Biquad HPF at 30 Hz: 10 kHz passes");

    // Reset clears state
    bq.setHighPass(100.0, 44100.0, 0.7071);
    for (int i = 0; i < 1000; ++i) bq.process(0.7f);
    bq.reset();
    check(bq.process(0.f) == 0.f, "Biquad reset: zero state after reset");
}

// ---- Hysteresis (JA model) --------------------------------------------------

static void test_hysteresis()
{
    DSP::Hysteresis ja;
    ja.setParams(1.f, 0.4f, 0.35f, 1.6e-3f);

    // Zero input -> zero output (starting from rest)
    check(ja.process(0.f) == 0.f, "Hysteresis: zero in -> zero out");

    // Bounded to [-Ms, Ms] = [-1, 1]
    ja.reset();
    bool bounded = true;
    for (int i = 0; i < 2000; ++i) {
        float x = std::sin(i * 0.05f) * 20.f; // large-amplitude input
        float y = ja.process(x);
        if (std::abs(y) > 1.01f) { bounded = false; break; }
    }
    check(bounded, "Hysteresis: output bounded to [-Ms, Ms]");

    // Positive small input -> positive output (no polarity flip)
    ja.reset();
    check(ja.process(0.001f) > 0.f, "Hysteresis: positive in -> positive out");

    // Odd hysteresis loop: for a symmetric sine, energy in odd harmonics
    // (verifiable by checking that process(x) is not exactly -process(-x) on
    //  the return path — due to the loop, this is expected to differ)
    // Here we just verify the small-signal DC component stays small after
    // a full symmetric cycle.
    ja.reset();
    float dcSum = 0.f;
    for (int i = 0; i < 4410; ++i) {
        float x = std::sin(2.f * 3.14159f * 100.f * i / 44100.f) * 0.3f;
        dcSum += ja.process(x);
    }
    check(std::abs(dcSum / 4410.f) < 0.01f, "Hysteresis: no DC bias on symmetric sine");
}

// ---- GainStage --------------------------------------------------------------

static void test_gainstage()
{
    DSP::GainStage gs;
    gs.prepare(44100.f);

    // With equal in/out signal, autoTrim should converge to ~1
    for (int i = 0; i < 50000; ++i) {
        float x = std::sin(i * 0.01f) * 0.5f;
        gs.update(x, x); // out == in
    }
    check(std::abs(gs.autoTrim - 1.f) < 0.05f, "GainStage: autoTrim -> 1 when out==in");

    // With 2x amplified output, autoTrim should converge toward ~0.5
    gs.rmsIn = gs.rmsOut = 0.f;
    gs.autoTrim = 1.f;
    for (int i = 0; i < 50000; ++i) {
        float x = std::sin(i * 0.01f) * 0.5f;
        gs.update(x, x * 2.f);
    }
    check(gs.autoTrim < 0.6f, "GainStage: autoTrim shrinks when out > in");
}

// ---- Saturation curve (THD check) -------------------------------------------

static void test_saturation_harmonics()
{
    // tubeSat has pre-gain: gain at small amplitude > gain at large amplitude.
    // Verify this saturation curve shape (small-signal gain > large-signal gain).
    const float drive = 0.6f;
    float gainSmall = std::abs(DSP::tubeSat(0.001f, drive)) / 0.001f; // ≈3.08×
    float gainLarge = std::abs(DSP::tubeSat(0.9f,  drive)) / 0.9f;   // < gainSmall
    check(gainSmall > gainLarge, "tubeSat: gain compression (small-sig gain > large-sig gain)");

    // Output is non-zero for non-zero input
    check(std::abs(DSP::tubeSat(0.5f, drive)) > 0.f, "tubeSat: non-zero output for non-zero input");

    // Large input: output bounded well below infinity
    check(std::abs(DSP::tubeSat(100.f, drive)) < 10.f, "tubeSat: bounded for extreme input");
}

// ---- Main -------------------------------------------------------------------

int main()
{
    printf("=== DSP Primitives Tests ===\n\n");

    test_tanhSat();
    test_tubeSat();
    test_tapeSat();
    test_hardClip();
    test_onepole();
    test_biquad();
    test_hysteresis();
    test_gainstage();
    test_saturation_harmonics();

    printf("\n--- %d passed, %d failed ---\n", passes, failures);
    return failures > 0 ? 1 : 0;
}
