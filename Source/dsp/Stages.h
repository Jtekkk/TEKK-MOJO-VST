#pragma once
#include "DSP.h"
#include <array>
#include <deque>
#include <vector>

namespace Stages
{
    struct MojoStage
    {
        bool bypass = false;
        virtual void prepare(double sampleRate, int maxBlockSize) = 0;
        virtual void reset() = 0;
        virtual float process(float x) = 0;
        virtual ~MojoStage() = default;
    };

    //==========================================================================
    // 1. Input Transformer  — Jiles-Atherton B-H hysteresis per iron type
    //==========================================================================
    struct InputTransformer : MojoStage
    {
        int   type      = 0;
        float drive     = 0.3f;
        float trimGain  = 1.f;
        bool  useAGC    = true;

        DSP::GainStage  gs;
        DSP::OnePole    dc;
        DSP::Hysteresis ja;
        double sr = 44100.0;
        int    cachedType = -1;

        // {Ms, a, k, alpha}  — Iron I: balanced · Iron II: wide/dark · Iron III: hifi/clean
        static void ironParams(int t, float& Ms, float& a, float& k, float& alpha)
        {
            switch (t) {
                case 1:  Ms=1.f; a=0.28f; k=0.55f; alpha=3.0e-3f; break;
                case 2:  Ms=1.f; a=0.60f; k=0.18f; alpha=7.0e-4f; break;
                default: Ms=1.f; a=0.40f; k=0.35f; alpha=1.6e-3f; break;
            }
        }

        void prepare(double sampleRate, int) override
        {
            sr = sampleRate;
            gs.prepare(static_cast<float>(sampleRate));
            dc.setFreq(5.f, static_cast<float>(sampleRate));
            updateJA();
        }

        void updateJA()
        {
            if (type == cachedType) return;
            cachedType = type;
            float Ms, a, k, alpha;
            ironParams(type, Ms, a, k, alpha);
            ja.setParams(Ms, a, k, alpha);
        }

        void reset() override
        {
            gs.rmsIn = gs.rmsOut = 0.f; gs.autoTrim = 1.f;
            dc.z1 = 0.f; ja.reset();
        }

        float process(float x) override
        {
            if (bypass) return x;
            updateJA();
            float preGain = 1.f + drive * 10.f;
            float M    = ja.process(x * preGain);
            float norm = ja.linGain() * preGain;          // unity in linear region
            float out  = M / std::max(norm, 1e-6f);
            float dcOut = out - dc.processLP(out);
            if (useAGC) { gs.update(x, dcOut); return dcOut * gs.autoTrim * trimGain; }
            return dcOut * trimGain;
        }
    };

    //==========================================================================
    // 2. High-Pass Filter
    //==========================================================================
    struct HighPassFilter : MojoStage
    {
        float freq  = 30.f;
        int   order = 1; // 0..3 = 1..4 poles

        std::array<DSP::Biquad, 2> filt;
        double sr = 44100.0;
        float  cachedFreq  = -1.f;
        int    cachedOrder = -1;

        void prepare(double sampleRate, int) override { sr = sampleRate; }
        void reset() override { for (auto& f : filt) f.reset(); }

        float process(float x) override
        {
            if (bypass) return x;
            if (freq != cachedFreq || order != cachedOrder)
            {
                cachedFreq  = freq;
                cachedOrder = order;
                // Butterworth Qs for 4-pole: 0.541, 1.307
                switch (order)
                {
                    case 0: filt[0].setHighPass(freq, sr, 0.7071); break;
                    case 1: filt[0].setHighPass(freq, sr, 0.7071);
                            filt[1].setHighPass(freq, sr, 0.7071); break;
                    case 2: filt[0].setHighPass(freq, sr, 0.5412);
                            filt[1].setHighPass(freq, sr, 1.3066); break;
                    case 3: filt[0].setHighPass(freq, sr, 0.5412);
                            filt[1].setHighPass(freq, sr, 1.3066); break;
                }
            }
            float y = filt[0].process(x);
            if (order >= 1) y = filt[1].process(y);
            return y;
        }
    };

    //==========================================================================
    // 3. Induction EQ (inductor-saturated 3-band)
    //==========================================================================
    struct InductionEQ : MojoStage
    {
        float lowFreq  = 100.f,  lowGain  = 0.f;
        float midFreq  = 1000.f, midGain  = 0.f, midQ = 0.7f;
        float highFreq = 8000.f, highGain = 0.f;
        float satDrive = 0.2f;

        DSP::Biquad low, mid, high;
        double sr = 44100.0;

        struct Cache { float lf, lg, mf, mg, mq, hf, hg; } cache{};

        void prepare(double sampleRate, int) override { sr = sampleRate; }
        void reset() override { low.reset(); mid.reset(); high.reset(); }

        float process(float x) override
        {
            if (bypass) return x;
            if (lowFreq!=cache.lf || lowGain!=cache.lg ||
                midFreq!=cache.mf || midGain!=cache.mg || midQ!=cache.mq ||
                highFreq!=cache.hf || highGain!=cache.hg)
            {
                low.setLowShelf (lowFreq,  sr, lowGain);
                mid.setPeakEQ   (midFreq,  sr, midGain, midQ);
                high.setHighShelf(highFreq, sr, highGain);
                cache = {lowFreq,lowGain,midFreq,midGain,midQ,highFreq,highGain};
            }
            float y = high.process(mid.process(low.process(x)));
            if (satDrive > 0.001f)
                y = DSP::tanhSat(y, satDrive * 0.25f) * (1.f / (1.f + satDrive * 0.15f));
            return y;
        }
    };

    //==========================================================================
    // 4. Compressor (FET / Opto / VCA / Vari-Mu)
    //==========================================================================
    struct Compressor : MojoStage
    {
        int   type      = 0;
        float threshold = -12.f;
        float ratio     = 4.f;
        float attackMs  = 10.f;
        float releaseMs = 100.f;
        float makeupDB  = 0.f;
        float mix       = 1.f;

        float env = 0.f;
        double sr = 44100.0;

        void prepare(double sampleRate, int) override { sr = sampleRate; env = 0.f; }
        void reset() override { env = 0.f; }

        float process(float x) override
        {
            if (bypass) return x;

            float attMult = 1.f;
            switch (type)
            {
                case 0: attMult = 1.0f;  break; // FET: native attack
                case 1: attMult = 5.0f;  break; // Opto: 5× slower attack
                case 2: attMult = 0.5f;  break; // VCA: slightly faster
                case 3: attMult = 10.f;  break; // Vari-Mu: very slow
            }
            float att = std::exp(-1.f / (static_cast<float>(sr) * attackMs  * 0.001f * attMult));
            float rel = std::exp(-1.f / (static_cast<float>(sr) * releaseMs * 0.001f));

            float absX = std::abs(x);
            env = absX > env ? att * env + (1.f - att) * absX
                             : rel * env + (1.f - rel) * absX;

            float thresh = std::pow(10.f, threshold / 20.f);
            float gr = 1.f;

            if (type == 3)
            {
                // Vari-Mu: soft knee — ratio increases gradually above threshold.
                // Knee width = 12 dB; below (thresh - 6 dB) → ratio 1:1.
                float envDB   = 20.f * std::log10(env + 1e-10f);
                float overDB  = envDB - threshold;
                if (overDB > -6.f)
                {
                    float knee = std::clamp((overDB + 6.f) / 12.f, 0.f, 1.f);
                    float effRatio = 1.f + (ratio - 1.f) * knee * knee;
                    float grDB = -overDB * (1.f - 1.f / effRatio);
                    gr = std::pow(10.f, grDB / 20.f);
                }
            }
            else if (env > thresh)
            {
                gr = std::pow(thresh / (env + 1e-10f), 1.f - 1.f / ratio);
            }

            float makeup = std::pow(10.f, makeupDB / 20.f);
            float wet    = x * gr * makeup;

            // Per-type harmonic flavour
            if (type == 0) wet = DSP::tanhSat(wet, 0.04f); // FET: slight grit
            if (type == 3) wet = DSP::tanhSat(wet, 0.07f); // Vari-Mu: warmth

            return wet * mix + x * (1.f - mix);
        }
    };

    //==========================================================================
    // 5. Tube + Tape Drive
    //    Tape path: head bump → HF pre-emphasis → JA saturation → HF de-emphasis
    //    Tube path: tubeSat (odd+even harmonics, kept as-is — sounds good)
    //==========================================================================
    struct TubeTapeDrive : MojoStage
    {
        int   mode     = 0;
        float amount   = 0.3f;
        float blend    = 1.f;
        float trimDB   = 0.f;
        bool  useAGC   = true;

        DSP::GainStage  gs;
        DSP::Hysteresis jaTape;
        // Tape colouring filters
        DSP::Biquad headBump;   // +3 dB peak ~80 Hz: low-end lift
        DSP::Biquad hfBoost;    // +4 dB shelf ~8 kHz: pre-emphasis (HF saturates first)
        DSP::Biquad hfCut;      // -4 dB shelf ~8 kHz: de-emphasis (post-sat)
        DSP::Biquad hfRolloff;  // -2 dB shelf ~14 kHz: playback HF loss
        double sr = 44100.0;
        double cachedSR = 0.0;

        // Tape JA params: lower coercivity than transformer iron
        static constexpr float kTapeMs=1.f, kTapeA=0.42f, kTapeK=0.22f, kTapeAlpha=1.1e-3f;

        void prepare(double sampleRate, int) override
        {
            sr = sampleRate;
            gs.prepare(static_cast<float>(sampleRate));
            if (sampleRate != cachedSR)
            {
                cachedSR = sampleRate;
                headBump .setPeakEQ    (80.0,    sr,  3.0, 0.75);
                hfBoost  .setHighShelf (8000.0,  sr,  4.0);
                hfCut    .setHighShelf (8000.0,  sr, -4.0);
                hfRolloff.setHighShelf (14000.0, sr, -2.0);
                jaTape.setParams(kTapeMs, kTapeA, kTapeK, kTapeAlpha);
            }
        }

        void reset() override
        {
            gs.rmsIn = gs.rmsOut = 0.f; gs.autoTrim = 1.f;
            headBump.reset(); hfBoost.reset(); hfCut.reset(); hfRolloff.reset();
            jaTape.reset();
        }

        float processTape(float x)
        {
            // Head bump (always on — defines tape identity)
            float bumped = headBump.process(x);
            // Pre-emphasis so HF enters saturation harder
            float preEmph = hfBoost.process(bumped);
            // JA saturation with tape oxide params
            float preGain = 1.f + amount * 6.f;
            float M    = jaTape.process(preEmph * preGain);
            float norm = jaTape.linGain() * preGain;
            float sat  = M / std::max(norm, 1e-6f);
            // De-emphasis + playback HF rolloff
            return hfRolloff.process(hfCut.process(sat));
        }

        float process(float x) override
        {
            if (bypass) return x;
            float wet = (mode == 1) ? processTape(x) : DSP::tubeSat(x, amount);
            float out  = wet * blend + x * (1.f - blend);
            float trim = std::pow(10.f, trimDB / 20.f);
            if (useAGC) { gs.update(x, out); return out * gs.autoTrim * trim; }
            return out * trim;
        }
    };

    //==========================================================================
    // 6. Output EQ (clean — no saturation)
    //==========================================================================
    struct OutputEQ : MojoStage
    {
        float lowFreq  = 100.f,  lowGain  = 0.f;
        float midFreq  = 1000.f, midGain  = 0.f, midQ = 0.7f;
        float highFreq = 8000.f, highGain = 0.f;

        DSP::Biquad low, mid, high;
        double sr = 44100.0;

        struct Cache { float lf, lg, mf, mg, mq, hf, hg; } cache{};

        void prepare(double sampleRate, int) override { sr = sampleRate; }
        void reset() override { low.reset(); mid.reset(); high.reset(); }

        float process(float x) override
        {
            if (bypass) return x;
            if (lowFreq!=cache.lf || lowGain!=cache.lg ||
                midFreq!=cache.mf || midGain!=cache.mg || midQ!=cache.mq ||
                highFreq!=cache.hf || highGain!=cache.hg)
            {
                low.setLowShelf (lowFreq,  sr, lowGain);
                mid.setPeakEQ   (midFreq,  sr, midGain, midQ);
                high.setHighShelf(highFreq, sr, highGain);
                cache = {lowFreq,lowGain,midFreq,midGain,midQ,highFreq,highGain};
            }
            return high.process(mid.process(low.process(x)));
        }
    };

    //==========================================================================
    // 7. Limiter — brick-wall with lookahead + true-peak detection
    //
    // Architecture:
    //   • Audio is delayed by kLookaheadMs so gain reduction is applied
    //     BEFORE the transient arrives → zero overshoot.
    //   • A monotonic-deque sliding minimum over the lookahead window gives
    //     the tightest required GR at each output sample in O(1) amortized.
    //   • Gain snaps down instantly (no attack artifacts) and releases at
    //     the user-set rate.
    //   • Bypass still passes the delayed signal so latency is constant.
    //   • latencyInOriginalSamples(osFactor) lets the engine report PDC.
    //==========================================================================
    struct Limiter : MojoStage
    {
        float threshDB  = -0.3f;
        float releaseMs = 50.f;
        static constexpr float kLookaheadMs = 2.f;

        double sr = 44100.0;
        int    lookaheadSamples = 0;
        int    bufSize = 1;
        int    writePos = 0;
        float  gainSmooth = 1.f;

        std::vector<float> audioBuf;

        // Monotonic deque: O(1) amortized sliding minimum over a moving window.
        struct SlidingMin
        {
            struct E { float v; int i; };
            std::deque<E> dq;
            int window = 0, tick = 0;

            void setWindow(int w) { window = w; reset(); }
            void reset()          { dq.clear(); tick = 0; }

            float push(float v)
            {
                while (!dq.empty() && dq.front().i <= tick - window) dq.pop_front();
                while (!dq.empty() && dq.back().v  >= v)             dq.pop_back();
                dq.push_back({v, tick++});
                return dq.front().v;
            }
        } smin;

        // Latency this stage adds, expressed at the original (non-OS) sample rate.
        int latencyInOriginalSamples(int osFactor) const
        {
            return (osFactor > 0) ? lookaheadSamples / osFactor : 0;
        }

        void prepare(double sampleRate, int) override
        {
            sr = sampleRate;
            lookaheadSamples = std::max(1,
                static_cast<int>(sampleRate * kLookaheadMs * 0.001));
            bufSize = lookaheadSamples + 1;
            audioBuf.assign(bufSize, 0.f);
            writePos  = 0;
            gainSmooth = 1.f;
            smin.setWindow(lookaheadSamples);
        }

        void reset() override
        {
            std::fill(audioBuf.begin(), audioBuf.end(), 0.f);
            writePos  = 0;
            gainSmooth = 1.f;
            smin.reset();
        }

        float process(float x) override
        {
            float thresh = std::pow(10.f, threshDB / 20.f);
            float rel    = std::exp(-1.f / (static_cast<float>(sr) * releaseMs * 0.001f));

            // Required gain for the current (future) sample — 1.0 when bypassed
            float absX     = std::abs(x);
            float grNeeded = (!bypass && absX > thresh)
                             ? thresh / (absX + 1e-10f) : 1.f;

            // Sliding min over the lookahead window
            float minGR = smin.push(grNeeded);

            // Write future audio, read delayed audio
            audioBuf[writePos] = x;
            int readPos = (writePos - lookaheadSamples + bufSize) % bufSize;
            float delayed = audioBuf[readPos];
            writePos = (writePos + 1) % bufSize;

            // Gain: instant attack (snap to minimum), exponential release
            gainSmooth = (minGR < gainSmooth)
                         ? minGR
                         : rel * gainSmooth + (1.f - rel) * 1.f;
            gainSmooth = std::clamp(gainSmooth, 0.f, 1.f);

            return delayed * gainSmooth;
        }
    };

    //==========================================================================
    // 8. Output Transformer  — same JA model, slightly softer params
    //==========================================================================
    struct OutputTransformer : MojoStage
    {
        int   type     = 0;
        float drive    = 0.2f;
        float trimDB   = 0.f;
        bool  useAGC   = true;

        DSP::GainStage  gs;
        DSP::OnePole    dc;
        DSP::Hysteresis ja;
        double sr = 44100.0;
        int    cachedType = -1;

        // Output xfmr typically has slightly narrower loop (less ringing)
        static void ironParams(int t, float& Ms, float& a, float& k, float& alpha)
        {
            switch (t) {
                case 1:  Ms=1.f; a=0.32f; k=0.48f; alpha=2.5e-3f; break;
                case 2:  Ms=1.f; a=0.65f; k=0.15f; alpha=6.0e-4f; break;
                default: Ms=1.f; a=0.45f; k=0.30f; alpha=1.2e-3f; break;
            }
        }

        void prepare(double sampleRate, int) override
        {
            sr = sampleRate;
            gs.prepare(static_cast<float>(sampleRate));
            dc.setFreq(5.f, static_cast<float>(sampleRate));
            updateJA();
        }

        void updateJA()
        {
            if (type == cachedType) return;
            cachedType = type;
            float Ms, a, k, alpha;
            ironParams(type, Ms, a, k, alpha);
            ja.setParams(Ms, a, k, alpha);
        }

        void reset() override
        {
            gs.rmsIn = gs.rmsOut = 0.f; gs.autoTrim = 1.f;
            dc.z1 = 0.f; ja.reset();
        }

        float process(float x) override
        {
            if (bypass) return x;
            updateJA();
            float preGain = 1.f + drive * 8.f;
            float M    = ja.process(x * preGain);
            float norm = ja.linGain() * preGain;
            float out  = M / std::max(norm, 1e-6f);
            float dcOut = out - dc.processLP(out);
            float trim  = std::pow(10.f, trimDB / 20.f);
            if (useAGC) { gs.update(x, dcOut); return dcOut * gs.autoTrim * trim; }
            return dcOut * trim;
        }
    };

    //==========================================================================
    // 9. Safety Clip
    //==========================================================================
    struct SafetyClip : MojoStage
    {
        float levelDB = -0.1f;

        void prepare(double, int) override {}
        void reset() override {}

        float process(float x) override
        {
            if (bypass) return x;
            return DSP::hardClip(x, std::pow(10.f, levelDB / 20.f));
        }
    };

} // namespace Stages
