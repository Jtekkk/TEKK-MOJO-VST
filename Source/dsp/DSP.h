#pragma once
#include <cmath>
#include <algorithm>

namespace DSP
{
    static constexpr double kTwoPi = 6.283185307179586;

    // ---- Saturation shapes ----

    inline float tanhSat(float x, float drive)
    {
        return std::tanh(x * (1.f + drive * 3.f));
    }

    // Tube: warm, odd + even harmonics
    inline float tubeSat(float x, float drive)
    {
        float g  = 1.f + drive * 5.f;
        float xd = x * g;
        return std::tanh(xd + 0.1f * xd * xd) / (1.f + drive * 0.5f);
    }

    inline float hardClip(float x, float ceil)
    {
        return std::clamp(x, -ceil, ceil);
    }

    // ---- One-pole smoother (LP) ----
    struct OnePole
    {
        float a1 = 0.f, z1 = 0.f;

        void setFreq(float freqHz, float sampleRate)
        {
            a1 = std::exp(static_cast<float>(-kTwoPi) * freqHz / sampleRate);
        }

        float processLP(float x)
        {
            z1 = x * (1.f - a1) + z1 * a1;
            return z1;
        }
    };

    // ---- Biquad (DF-II transposed, double precision) ----
    struct Biquad
    {
        double b0=1, b1=0, b2=0, a1=0, a2=0;
        double z1=0, z2=0;

        void reset() { z1 = z2 = 0.0; }

        void setPeakEQ(double freq, double sr, double gainDB, double Q)
        {
            double A  = std::pow(10.0, gainDB / 40.0);
            double w0 = kTwoPi * freq / sr;
            double cw = std::cos(w0), sw = std::sin(w0);
            double al = sw / (2.0 * Q);
            double a0 = 1.0 + al / A;
            b0 = (1.0 + al * A) / a0;
            b1 = (-2.0 * cw)    / a0;
            b2 = (1.0 - al * A) / a0;
            a1 = (-2.0 * cw)    / a0;
            a2 = (1.0 - al / A) / a0;
        }

        void setLowShelf(double freq, double sr, double gainDB, double slope = 1.0)
        {
            double A  = std::pow(10.0, gainDB / 40.0);
            double w0 = kTwoPi * freq / sr;
            double cw = std::cos(w0), sw = std::sin(w0);
            double al = sw / 2.0 * std::sqrt((A + 1.0/A) * (1.0/slope - 1.0) + 2.0);
            double sA = std::sqrt(A);
            double a0 = (A+1) + (A-1)*cw + 2*sA*al;
            b0 =  A * ((A+1) - (A-1)*cw + 2*sA*al) / a0;
            b1 =  2*A * ((A-1) - (A+1)*cw)          / a0;
            b2 =  A * ((A+1) - (A-1)*cw - 2*sA*al) / a0;
            a1 = -2 * ((A-1) + (A+1)*cw)            / a0;
            a2 =  ((A+1) + (A-1)*cw - 2*sA*al)      / a0;
        }

        void setHighShelf(double freq, double sr, double gainDB, double slope = 1.0)
        {
            double A  = std::pow(10.0, gainDB / 40.0);
            double w0 = kTwoPi * freq / sr;
            double cw = std::cos(w0), sw = std::sin(w0);
            double al = sw / 2.0 * std::sqrt((A + 1.0/A) * (1.0/slope - 1.0) + 2.0);
            double sA = std::sqrt(A);
            double a0 = (A+1) - (A-1)*cw + 2*sA*al;
            b0 =  A * ((A+1) + (A-1)*cw + 2*sA*al) / a0;
            b1 = -2*A * ((A-1) + (A+1)*cw)          / a0;
            b2 =  A * ((A+1) + (A-1)*cw - 2*sA*al) / a0;
            a1 =  2 * ((A-1) - (A+1)*cw)            / a0;
            a2 =  ((A+1) - (A-1)*cw - 2*sA*al)      / a0;
        }

        void setHighPass(double freq, double sr, double Q = 0.7071)
        {
            double w0 = kTwoPi * freq / sr;
            double cw = std::cos(w0), sw = std::sin(w0);
            double al = sw / (2.0 * Q);
            double a0 = 1.0 + al;
            b0 =  (1.0 + cw) / (2.0 * a0);
            b1 = -(1.0 + cw) / a0;
            b2 =  (1.0 + cw) / (2.0 * a0);
            a1 = (-2.0 * cw) / a0;
            a2 = (1.0 - al)  / a0;
        }

        float process(float x)
        {
            double y = b0*x + z1;
            z1 = b1*x - a1*y + z2;
            z2 = b2*x - a2*y;
            return static_cast<float>(y);
        }
    };

    // ---- Jiles-Atherton magnetic hysteresis (simplified, per-sample Euler) ----
    // Models the B-H loop of transformer iron or tape oxide.
    // Output M ∈ [-Ms, Ms]; normalize by Ms/(a·k) for unity small-signal gain.
    struct Hysteresis
    {
        float M      = 0.f;
        float H_prev = 0.f;
        float Ms = 1.f, a = 0.40f, k = 0.35f, alpha = 1.6e-3f;

        void setParams(float ms, float aVal, float kVal, float alphaVal)
        {
            Ms = ms; a = aVal; k = kVal; alpha = alphaVal;
        }

        void reset() { M = 0.f; H_prev = 0.f; }

        // linGain() returns small-signal dM/dH so callers can normalize
        float linGain() const { return Ms / std::max(a * k + alpha * Ms, 1e-10f); }

        float process(float H)
        {
            float dH  = H - H_prev;
            float He  = H + alpha * M;
            float Man = Ms * std::tanh(He / std::max(a, 1e-6f));
            float sign  = (dH >= 0.f) ? 1.f : -1.f;
            float denom = k * sign - alpha * (Man - M);
            // Guard against zero crossing (direction reversal) instability
            if (std::abs(denom) < 1e-7f)
                denom = std::copysign(1e-7f, denom);
            M = std::clamp(M + (Man - M) / denom * dH, -Ms, Ms);
            H_prev = H;
            return M;
        }
    };

    // ---- GainStage: drive pre-gain + auto-gain trim ----
    struct GainStage
    {
        float rmsIn    = 0.f;
        float rmsOut   = 0.f;
        float autoTrim = 1.f;
        float rmsCoeff = 0.f;

        void prepare(float sampleRate)
        {
            rmsCoeff = std::exp(-1.f / (sampleRate * 0.3f));
        }

        void update(float inSample, float outSample)
        {
            rmsIn  = rmsCoeff * rmsIn  + (1.f - rmsCoeff) * inSample  * inSample;
            rmsOut = rmsCoeff * rmsOut + (1.f - rmsCoeff) * outSample * outSample;
            if (rmsOut > 1e-10f)
                autoTrim = std::sqrt(std::max(rmsIn, 1e-10f) / rmsOut);
            autoTrim = std::clamp(autoTrim, 0.125f, 8.f);
        }
    };

} // namespace DSP
