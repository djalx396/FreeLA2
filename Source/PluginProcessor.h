#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "LA2Engine.h"
#include "PhaseAlign.h"

#ifndef FREELA2_BUILD
 #define FREELA2_BUILD "local"
#endif

namespace ID
{
    static const juce::String power { "power" }, pushed { "pushed" }, drive { "drive" }, level { "level" },
        limit { "limit" }, reduction { "reduction" }, emphasis { "emphasis" }, gain { "gain" },
        ahead { "ahead" }, clipdrive { "clipdrive" }, clipout { "clipout" }, mix { "mix" }, filter { "filter" },
        oslive { "oslive" }, osrender { "osrender" }, rendermode { "rendermode" }, meter { "meter" }, finish { "finish" },
        phase { "phase" }, prestage { "prestage" }, levstage { "levstage" }, clipstage { "clipstage" };
}

class FreeLA2Processor : public juce::AudioProcessor
{
public:
    FreeLA2Processor();
    ~FreeLA2Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "FreeLA2"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // for the editor
    std::atomic<float> grMeter { 0.f };        // dB, max over the last block
    std::atomic<float> outRms { 0.f };         // linear, RMS of the last block (both channels)
    std::atomic<float> clipLevel { 0.f };      // >= 1 when the clipper is clipping
    std::atomic<int>   activeFactor { 1 };
    std::atomic<bool>  filtersOn { false };
    std::atomic<int>   latencyNow { 0 };
    std::atomic<int>   testStages { 7 };           // tests only (not a parameter): 1 Pre, 2 Leveler, 4 Clipper
    int maxFactorForRate() const { return maxFactor; }
    static juce::String versionString() { return juce::String ("v") + JucePlugin_VersionString + "  build " + FREELA2_BUILD; }

    enum PIdx { pPower, pPushed, pDrive, pLevel, pLimit, pReduction, pEmphasis, pGain, pAhead, pClipDrive, pClipOut,
                pMix, pFilter, pOsLive, pOsRender, pRenderMode, pPhase, pPreStage, pLevStage, pClipStage, pCount };
    static constexpr double kAheadMs[5] = { 0.0, 1.5, 3.0, 4.5, 6.0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    la2::Settings readSettings() const;
    int wantedFactor (bool offline) const;
    void updateLatency();

    static constexpr int kFactors[4] = { 1, 2, 4, 8 };
    struct OsSet
    {
        int factor = 1;
        std::unique_ptr<juce::dsp::Oversampling<double>> os;   // null for factor 1
        la2::Engine eng;
        // alignment of the linear part (oversampling filters + band-limit sections), per filter mode OFF / CLEAN / AIR
        double tau[3] { 0, 0, 0 };                             // measured low-frequency delay, session-rate samples
        std::vector<double> fir[2][3];                         // [ANALOG / LINEAR][mode] output FIR
        int pad[2][3] {};                                      // whole-sample padding after the FIR
    };
    std::array<OsSet, 4> sets;
    int activeSet = 0, maxFactor = 8, numCh = 2, maxBlock = 512;
    int fixedLat[2] { 0, 0 };                                  // ANALOG / LINEAR (without AHEAD)
    void measureLinear (OsSet& set, int mode, std::vector<double>& h);
    double fs = 48000;
    la2::Model model;

    struct Delay
    {
        std::vector<double> buf; int w = 0;
        void prepare (int maxDelay) { buf.assign ((size_t) maxDelay + 1, 0.0); w = 0; }
        inline double process (double x, int d) noexcept
        {
            if (d <= 0 || buf.empty()) return x;
            buf[(size_t) w] = x;
            int r = w - d; if (r < 0) r += (int) buf.size();
            w = (w + 1) % (int) buf.size();
            return buf[(size_t) r];
        }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0); }
    };
    Delay padDelay[2], bypassDelay[2];
    juce::AudioBuffer<double> work, dryBuf;
    int lastAheadIdx = -1, lastPhase = -1;
    pa::Fir outFir[2];
    bool wasOn = true;
    juce::SmoothedValue<double> bypassMix;     // 1 = processed, 0 = bypassed (click-free power switch)

    std::atomic<float>* p[pCount] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreeLA2Processor)
};
