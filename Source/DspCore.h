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
        float xover1Hz = 250.0f;
        float xover2Hz = 800.0f;
        float xover3Hz = 2500.0f;
    };

    void prepare (double newSampleRate)
    {
        fs = newSampleRate;
        reset();
        splitter.prepare (fs, 250.0f, 800.0f, 2500.0f);
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
        static constexpr std::array<float, 4> rel0 { 10.0f, 11.9f, 5.1f, 3.0f };

        splitter.setCrossovers (p.xover1Hz, p.xover2Hz, p.xover3Hz);

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
        void setCutoff (float fc)
        {
            fc = juce::jlimit (20.0f, (float) sampleRate * 0.45f, fc);
            if (std::abs (fc - cutoff) > 0.01f)
            {
                cutoff = fc;
                update();
            }
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
        void setCutoff (float fc) { lp.setCutoff (fc); hp.setCutoff (fc); }
        void reset() { lp.reset(); hp.reset(); }
        float process (float x) { return lp.process (x) + hp.process (x); }
    };

    struct Splitter
    {
        LR4 lpMid, hpMid;
        LR4 b1lpLow, b2hpLow, b3lpHigh, b4hpHigh;
        PhasePair b1phaseHigh, b2phaseHigh, b3phaseLow, b4phaseLow;
        float lowFc = 250.0f, midFc = 800.0f, highFc = 2500.0f;

        void prepare (double sr, float low, float mid, float high)
        {
            lowFc = low; midFc = mid; highFc = high;
            lpMid.prepare (sr, midFc, false); hpMid.prepare (sr, midFc, true);
            b1lpLow.prepare (sr, lowFc, false); b2hpLow.prepare (sr, lowFc, true);
            b3lpHigh.prepare (sr, highFc, false); b4hpHigh.prepare (sr, highFc, true);
            b1phaseHigh.prepare (sr, highFc); b2phaseHigh.prepare (sr, highFc);
            b3phaseLow.prepare (sr, lowFc); b4phaseLow.prepare (sr, lowFc);
        }
        void setCrossovers (float low, float mid, float high)
        {
            // Keep the three crossovers ordered even during automation.
            low = juce::jlimit (60.0f, 700.0f, low);
            mid = juce::jlimit (low + 40.0f, 1800.0f, mid);
            high = juce::jlimit (mid + 100.0f, 7000.0f, high);

            if (std::abs (low - lowFc) > 0.01f)
            {
                lowFc = low;
                b1lpLow.setCutoff (lowFc); b2hpLow.setCutoff (lowFc);
                b3phaseLow.setCutoff (lowFc); b4phaseLow.setCutoff (lowFc);
            }
            if (std::abs (mid - midFc) > 0.01f)
            {
                midFc = mid;
                lpMid.setCutoff (midFc); hpMid.setCutoff (midFc);
            }
            if (std::abs (high - highFc) > 0.01f)
            {
                highFc = high;
                b3lpHigh.setCutoff (highFc); b4hpHigh.setCutoff (highFc);
                b1phaseHigh.setCutoff (highFc); b2phaseHigh.setCutoff (highFc);
            }
        }
        void reset()
        {
            lpMid.reset(); hpMid.reset(); b1lpLow.reset(); b2hpLow.reset(); b3lpHigh.reset(); b4hpHigh.reset();
            b1phaseHigh.reset(); b2phaseHigh.reset(); b3phaseLow.reset(); b4phaseLow.reset();
        }
        void process (float x, std::array<float,4>& out)
        {
            const float lo = lpMid.process (x);
            const float hi = hpMid.process (x);
            out[0] = b1phaseHigh.process (b1lpLow.process (lo));
            out[1] = b2phaseHigh.process (b2hpLow.process (lo));
            out[2] = b3phaseLow.process (b3lpHigh.process (hi));
            out[3] = b4phaseLow.process (b4hpHigh.process (hi));
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
