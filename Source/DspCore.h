#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>

class SteveSledgeDspCore
{
public:
    struct Params
    {
        float inputGainDb = 0.0f;
        float thresholdDb = -29.0f;
        float ratio = 4.0f;
        float speed = 1.0f;
        float makeupDb = 15.0f;
    };

    void prepare (double newSampleRate)
    {
        fs = newSampleRate;
        reset();
        splitter.prepare (fs);
    }

    void reset()
    {
        splitter.reset();
        for (auto& b : bandState) b = {};
    }

    float processSample (float x, const Params& p)
    {
        constexpr float eps = 1.0e-10f;
        constexpr float driveDb = 6.0f;
        constexpr float detAttackMs = 2.0f;
        constexpr float persistAttackMs = 70.0f;
        constexpr float persistReleaseMs = 18.0f;
        constexpr float grStartDb = 1.5f;
        constexpr float grFullDb = 10.0f;
        constexpr float persistExp = 4.0f;
        constexpr float balMs = 20.0f;

        static constexpr std::array<float, 4> att0 { 2.5f, 2.975f, 1.275f, 0.425f };
        static constexpr std::array<float, 4> rel0 { 10.0f, 11.9f, 5.1f, 1.7f };

        const float inputGain = juce::Decibels::decibelsToGain (p.inputGainDb);
        const float drive = juce::Decibels::decibelsToGain (driveDb);
        const float driven = x * inputGain * drive;

        std::array<float, 4> bands {};
        splitter.process (driven, bands);

        std::array<float, 4> comp {};
        std::array<float, 4> balanceGain {};
        std::array<float, 4> eout {};

        const float balA = coeffMs (balMs);
        const float speed = juce::jlimit (0.1f, 10.0f, p.speed);

        for (size_t k = 0; k < 4; ++k)
        {
            auto& st = bandState[k];
            const float originalBand = bands[k] / drive;

            const float qdet = square (originalBand * drive);
            const float ae = coeffMs (detAttackMs / speed);
            st.persistEnv = ae * st.persistEnv + (1.0f - ae) * qdet;
            const float levDet = 10.0f * std::log10 (std::max (st.persistEnv, eps));
            const float gm = levDet > p.thresholdDb
                               ? levDet - (p.thresholdDb + (levDet - p.thresholdDb) / p.ratio)
                               : 0.0f;
            float u = (gm - grStartDb) / (grFullDb - grStartDb);
            u = juce::jlimit (0.0f, 1.0f, u);
            u = u * u * (3.0f - 2.0f * u);
            const float ap = coeffMs ((u > st.persistState ? persistAttackMs : persistReleaseMs) / speed);
            st.persistState = ap * st.persistState + (1.0f - ap) * u;
            const float releaseFactor = 1.0f + 9.0f * std::pow (juce::jlimit (0.0f, 1.0f, st.persistState), persistExp);

            const float attackMs = att0[k] / speed;
            const float releaseMs = rel0[k] * releaseFactor / speed;
            const float aa = coeffMs (attackMs);
            const float ar = coeffMs (releaseMs);
            const float q = square (bands[k]);
            const float envA = q > st.compEnv ? aa : ar;
            st.compEnv = envA * st.compEnv + (1.0f - envA) * q;
            const float lev = 10.0f * std::log10 (std::max (st.compEnv, eps));
            const float tgt = lev > p.thresholdDb
                                ? (p.thresholdDb + (lev - p.thresholdDb) / p.ratio - lev)
                                : 0.0f;
            const float gainA = tgt < st.gainDb ? aa : ar;
            st.gainDb = gainA * st.gainDb + (1.0f - gainA) * tgt;
            comp[k] = bands[k] * juce::Decibels::decibelsToGain (st.gainDb) / drive;

            const float qi = square (originalBand);
            const float qo = square (comp[k]);
            if (! st.energyInitialised)
            {
                st.ein = qi;
                st.eout = qo;
                st.energyInitialised = true;
            }
            else
            {
                st.ein = balA * st.ein + (1.0f - balA) * qi;
                st.eout = balA * st.eout + (1.0f - balA) * qo;
            }
            eout[k] = st.eout;
        }

        for (size_t k = 0; k < 4; ++k)
        {
            const float raw = (10.0f * std::log10 (std::max (bandState[k].ein, eps))
                             - 10.0f * std::log10 (std::max (bandState[2].ein, eps)))
                            - (10.0f * std::log10 (std::max (bandState[k].eout, eps))
                             - 10.0f * std::log10 (std::max (bandState[2].eout, eps)));
            balanceGain[k] = juce::Decibels::decibelsToGain (raw);
        }

        float num = 0.0f, den = 0.0f;
        for (size_t k = 0; k < 4; ++k)
        {
            num += eout[k];
            den += eout[k] * square (balanceGain[k]);
        }
        const float common = std::sqrt (num / std::max (den, eps));
        const float makeup = juce::Decibels::decibelsToGain (p.makeupDb);

        float sum = 0.0f;
        for (size_t k = 0; k < 4; ++k)
            sum += comp[k] * balanceGain[k] * common * makeup;

        return sum;
    }

    float getBandGainReductionDb (size_t band) const
    {
        return band < bandState.size() ? std::max (0.0f, -bandState[band].gainDb) : 0.0f;
    }

private:
    struct LR4
    {
        juce::dsp::IIR::Filter<float> a, b;
        bool high = false;
        double sampleRate = 48000.0;
        float cutoff = 1000.0f;

        void prepare (double sr, float fc, bool isHigh)
        {
            sampleRate = sr; cutoff = fc; high = isHigh;
            update(); reset();
        }
        void update()
        {
            auto c = high ? juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, cutoff, 0.70710678118f)
                          : juce::dsp::IIR::Coefficients<float>::makeLowPass  (sampleRate, cutoff, 0.70710678118f);
            *a.coefficients = *c;
            *b.coefficients = *c;
        }
        void reset() { a.reset(); b.reset(); }
        float process (float x) { return b.processSample (a.processSample (x)); }
    };

    struct PhasePair
    {
        LR4 lp, hp;
        void prepare (double sr, float fc) { lp.prepare (sr, fc, false); hp.prepare (sr, fc, true); }
        void reset() { lp.reset(); hp.reset(); }
        float process (float x) { return lp.process (x) + hp.process (x); }
    };

    struct Splitter
    {
        LR4 lp800, hp800;
        LR4 b1lp250, b2hp250, b3lp2500, b4hp2500;
        PhasePair b1phase2500, b2phase2500, b3phase250, b4phase250;

        void prepare (double sr)
        {
            lp800.prepare (sr, 800.0f, false); hp800.prepare (sr, 800.0f, true);
            b1lp250.prepare (sr, 250.0f, false); b2hp250.prepare (sr, 250.0f, true);
            b3lp2500.prepare (sr, 2500.0f, false); b4hp2500.prepare (sr, 2500.0f, true);
            b1phase2500.prepare (sr, 2500.0f); b2phase2500.prepare (sr, 2500.0f);
            b3phase250.prepare (sr, 250.0f); b4phase250.prepare (sr, 250.0f);
        }
        void reset()
        {
            lp800.reset(); hp800.reset(); b1lp250.reset(); b2hp250.reset(); b3lp2500.reset(); b4hp2500.reset();
            b1phase2500.reset(); b2phase2500.reset(); b3phase250.reset(); b4phase250.reset();
        }
        void process (float x, std::array<float,4>& out)
        {
            const float lo = lp800.process (x);
            const float hi = hp800.process (x);
            out[0] = b1phase2500.process (b1lp250.process (lo));
            out[1] = b2phase2500.process (b2hp250.process (lo));
            out[2] = b3phase250.process (b3lp2500.process (hi));
            out[3] = b4phase250.process (b4hp2500.process (hi));
        }
    };

    struct BandState
    {
        float persistEnv = 0.0f;
        float persistState = 0.0f;
        float compEnv = 0.0f;
        float gainDb = 0.0f;
        float ein = 0.0f;
        float eout = 0.0f;
        bool energyInitialised = false;
    };

    float coeffMs (float ms) const
    {
        return std::exp (-1.0f / static_cast<float> (fs * std::max (ms, 0.001f) * 0.001));
    }
    static float square (float x) { return x * x; }

    double fs = 48000.0;
    Splitter splitter;
    std::array<BandState, 4> bandState {};
};
