#pragma once
#include "DSP.h"
#include <array>

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
    // 1. Input Transformer
    //==========================================================================
    struct InputTransformer : MojoStage
    {
        int   type      = 0;
        float drive     = 0.3f;
        float trimGain  = 1.f;
        bool  useAGC    = true;

        DSP::GainStage gs;
        DSP::OnePole   dc;
        double sr = 44100.0;

        void prepare(double sampleRate, int) override
        {
            sr = sampleRate;
            gs.prepare(static_cast<float>(sampleRate));
            dc.setFreq(5.f, static_cast<float>(sampleRate));
        }

        void reset() override
        {
            gs.rmsIn = gs.rmsOut = gs.autoTrim = 0.f;
            gs.autoTrim = 1.f;
            dc.z1 = 0.f;
        }

        float process(float x) override
        {
            if (bypass) return x;
            float xd = x * (1.f + drive * 12.f);
            float shaped;
            switch (type)
            {
                case 1:  shaped = DSP::asymTanh(xd, drive, 0.8f);  break; // Iron II: thick
                case 2:  shaped = DSP::asymTanh(xd, drive, 0.15f); break; // Iron III: subtle
                default: shaped = DSP::asymTanh(xd, drive, 0.4f);  break; // Iron I
            }
            float out = shaped - dc.processLP(shaped); // DC block
            if (useAGC)
            {
                gs.update(x, out);
                return out * gs.autoTrim * trimGain;
            }
            return out * trimGain;
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
            if (env > thresh)
                gr = std::pow(thresh / (env + 1e-10f), 1.f - 1.f / ratio);

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
    //==========================================================================
    struct TubeTapeDrive : MojoStage
    {
        int   mode     = 0;   // 0=Tube, 1=Tape
        float amount   = 0.3f;
        float blend    = 1.f;
        float trimDB   = 0.f;
        bool  useAGC   = true;

        DSP::GainStage gs;
        double sr = 44100.0;

        void prepare(double sampleRate, int) override
        {
            sr = sampleRate;
            gs.prepare(static_cast<float>(sampleRate));
        }

        void reset() override
        {
            gs.rmsIn = gs.rmsOut = 0.f;
            gs.autoTrim = 1.f;
        }

        float process(float x) override
        {
            if (bypass) return x;
            float wet = (mode == 1) ? DSP::tapeSat(x, amount) : DSP::tubeSat(x, amount);
            float out = wet * blend + x * (1.f - blend);
            float trim = std::pow(10.f, trimDB / 20.f);
            if (useAGC)
            {
                gs.update(x, out);
                return out * gs.autoTrim * trim;
            }
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
    // 7. Limiter (brick-wall, peak-hold release)
    //==========================================================================
    struct Limiter : MojoStage
    {
        float threshDB  = -0.3f;
        float releaseMs = 50.f;
        float env       = 0.f;
        double sr = 44100.0;

        void prepare(double sampleRate, int) override { sr = sampleRate; env = 0.f; }
        void reset() override { env = 0.f; }

        float process(float x) override
        {
            if (bypass) return x;
            float rel    = std::exp(-1.f / (static_cast<float>(sr) * releaseMs * 0.001f));
            float thresh = std::pow(10.f, threshDB / 20.f);
            float absX   = std::abs(x);
            env = (absX > env) ? absX : rel * env + (1.f - rel) * absX;
            float gr = (env > thresh) ? thresh / (env + 1e-10f) : 1.f;
            return x * gr;
        }
    };

    //==========================================================================
    // 8. Output Transformer
    //==========================================================================
    struct OutputTransformer : MojoStage
    {
        int   type     = 0;
        float drive    = 0.2f;
        float trimDB   = 0.f;
        bool  useAGC   = true;

        DSP::GainStage gs;
        DSP::OnePole   dc;
        double sr = 44100.0;

        void prepare(double sampleRate, int) override
        {
            sr = sampleRate;
            gs.prepare(static_cast<float>(sampleRate));
            dc.setFreq(5.f, static_cast<float>(sampleRate));
        }

        void reset() override
        {
            gs.rmsIn = gs.rmsOut = 0.f;
            gs.autoTrim = 1.f;
            dc.z1 = 0.f;
        }

        float process(float x) override
        {
            if (bypass) return x;
            float xd = x * (1.f + drive * 10.f);
            float shaped;
            switch (type)
            {
                case 1:  shaped = DSP::asymTanh(xd, drive, 0.6f);  break;
                case 2:  shaped = DSP::asymTanh(xd, drive, 0.1f);  break;
                default: shaped = DSP::asymTanh(xd, drive, 0.3f);  break;
            }
            float out = shaped - dc.processLP(shaped);
            float trim = std::pow(10.f, trimDB / 20.f);
            if (useAGC)
            {
                gs.update(x, out);
                return out * gs.autoTrim * trim;
            }
            return out * trim;
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
