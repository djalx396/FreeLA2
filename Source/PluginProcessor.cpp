#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ModelData.h"

using APF = juce::AudioParameterFloat;
using APC = juce::AudioParameterChoice;
using APB = juce::AudioParameterBool;

juce::AudioProcessorValueTreeState::ParameterLayout FreeLA2Processor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> v;
    auto db  = [] (float x, int) { return (x > 0.f ? "+" : "") + juce::String (x, 1) + " dB"; };
    auto one = [] (float x, int) { return juce::String (x, 1); };
    auto pc  = [] (float x, int) { return juce::String (juce::roundToInt (x)) + " %"; };
    auto f = [&] (const juce::String& id, const juce::String& name, float lo, float hi, float step, float def,
                  std::function<juce::String (float, int)> fn)
    {
        v.push_back (std::make_unique<APF> (juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (lo, hi, step), def,
                                            juce::AudioParameterFloatAttributes().withStringFromValueFunction (fn)));
    };
    auto c = [&] (const juce::String& id, const juce::String& name, juce::StringArray items, int def, bool autom = true)
    {
        v.push_back (std::make_unique<APC> (juce::ParameterID { id, 1 }, name, items, def,
                                            juce::AudioParameterChoiceAttributes().withAutomatable (autom)));
    };
    v.push_back (std::make_unique<APB> (juce::ParameterID { ID::power, 1 }, "Power", true));
    c (ID::pushed, "Pre Program", { "NORMAL", "PUSHED" }, 0);
    f (ID::drive, "Pre Drive", -30.f, 30.f, 0.1f, 0.f, one);
    f (ID::level, "Pre Output (Leveler Input)", -18.f, 18.f, 0.01f, 0.f, db);
    c (ID::limit, "Leveler Mode", { "COMP", "LIMIT" }, 0);
    f (ID::reduction, "Peak Reduction", 0.f, 100.f, 0.1f, 0.f, one);
    f (ID::emphasis, "Emphasis", -100.f, 100.f, 0.1f, 0.f,
       [] (float x, int) { return std::abs (x) < 0.5f ? juce::String ("Centre") : (x < 0 ? "Fast " : "Slow bass ") + juce::String (std::abs (x), 0); });
    f (ID::gain, "Gain", 0.f, 25.f, 0.01f, 0.f, db);
    c (ID::ahead, "Ahead", { "0 ms", "1.5 ms", "3 ms", "4.5 ms", "6 ms" }, 0);
    f (ID::clipdrive, "Clipper Drive", 0.f, 40.f, 0.01f, 0.f, one);
    f (ID::clipout, "Clipper Output", 0.f, 10.f, 0.01f, 0.f, db);
    f (ID::mix, "Leveler Mix", 0.f, 100.f, 0.1f, 100.f, pc);
    c (ID::filter, "Band-limit", { "OFF", "CLEAN", "AIR" }, 1);
    c (ID::oslive, "Oversampling", { "Off", "2x", "4x", "8x" }, 1, false);
    c (ID::osrender, "Render oversampling", { "Off", "2x", "4x", "8x" }, 2, false);
    c (ID::rendermode, "Use render oversampling", { "Auto (when rendering)", "Always", "Never" }, 0, false);
    c (ID::meter, "Meter", { "GR", "OUT" }, 0, false);
    c (ID::finish, "Finish", { "Enamel", "Hammertone", "Midnight" }, 0, false);
    c (ID::phase, "Filter Phase", { "ANALOG", "LINEAR" }, 0, false);
    return { v.begin(), v.end() };
}

FreeLA2Processor::FreeLA2Processor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FreeLA2", createLayout())
{
    const juce::String ids[pCount] = { ID::power, ID::pushed, ID::drive, ID::level, ID::limit, ID::reduction,
        ID::emphasis, ID::gain, ID::ahead, ID::clipdrive, ID::clipout, ID::mix, ID::filter, ID::oslive, ID::osrender, ID::rendermode, ID::phase };
    for (int i = 0; i < pCount; ++i) p[i] = apvts.getRawParameterValue (ids[i]);
    model = la2::makeModel();
}

bool FreeLA2Processor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (in != out) return false;
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

la2::Settings FreeLA2Processor::readSettings() const
{
    la2::Settings s;
    const int st = testStages.load();
    s.preOn = (st & 1) != 0; s.compOn = (st & 2) != 0; s.clipOn = (st & 4) != 0;
    s.pushed     = juce::roundToInt (p[pPushed]->load()) == 1;
    s.drive      = p[pDrive]->load();
    s.levelDb    = p[pLevel]->load();
    s.limit      = juce::roundToInt (p[pLimit]->load()) == 1;
    s.reduction  = p[pReduction]->load();
    s.emphasis   = p[pEmphasis]->load();
    s.gainDb     = p[pGain]->load();
    s.aheadMs    = kAheadMs[juce::jlimit (0, 4, juce::roundToInt (p[pAhead]->load()))];
    s.clipIn     = p[pClipDrive]->load();
    s.clipOutDb  = p[pClipOut]->load();
    s.mix        = p[pMix]->load() / 100.0;
    s.filterMode = juce::roundToInt (p[pFilter]->load());
    return s;
}

int FreeLA2Processor::wantedFactor (bool offline) const
{
    const int mode = juce::roundToInt (p[pRenderMode]->load());     // 0 Auto, 1 Always, 2 Never
    const bool useRender = mode == 1 || (mode == 0 && offline);
    const int idx = juce::roundToInt ((useRender ? p[pOsRender] : p[pOsLive])->load());
    return juce::jmin (kFactors[juce::jlimit (0, 3, idx)], maxFactor);
}

// impulse response (session rate) of the linear part that the output alignment corrects:
// up-sampling -> every band-limit section of the mode in series -> down-sampling
void FreeLA2Processor::measureLinear (OsSet& set, int mode, std::vector<double>& h)
{
    const double fo = fs * set.factor;
    fbl::BandLimit<4> bl;
    if (mode > 0 && fo >= 88000.0) bl.configure (la2::layoutSpecs (mode), fo); else bl.off();
    int len = 4096; while (len < (int) std::ceil (0.04 * fs)) len <<= 1;      // >= 40 ms
    h.assign ((size_t) len, 0.0);
    juce::AudioBuffer<double> tmp (numCh, maxBlock);
    if (set.os) set.os->reset();
    for (int pos = 0; pos < len; pos += maxBlock)
    {
        const int n = juce::jmin (maxBlock, len - pos);
        tmp.clear();
        if (pos == 0) tmp.setSample (0, 0, 1.0);
        juce::dsp::AudioBlock<double> block (tmp.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
        juce::dsp::AudioBlock<double> up = set.os ? set.os->processSamplesUp (block) : block;
        double* u = up.getChannelPointer (0);
        for (size_t i = 0; i < up.getNumSamples(); ++i) u[i] = bl.processDry (u[i]);
        if (set.os) set.os->processSamplesDown (block);
        for (int i = 0; i < n; ++i) h[(size_t) (pos + i)] = tmp.getSample (0, i);
    }
    if (set.os) set.os->reset();
}

void FreeLA2Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    maxBlock = juce::jmax (1, samplesPerBlock);
    numCh = juce::jmax (1, juce::jmin (2, getTotalNumOutputChannels()));
    maxFactor = fs <= 100000.0 ? 8 : 4;                 // up to 768 kHz inside
    const auto s = readSettings();
    const int maxAhead = (int) std::ceil (kAheadMs[4] * 1e-3 * fs) + 1;

    std::vector<double> hs[4][3];
    for (int i = 0; i < 4; ++i)
    {
        auto& set = sets[(size_t) i];
        set.factor = kFactors[i];
        set.os.reset();
        if (set.factor > maxFactor) continue;
        if (set.factor > 1)
        {
            set.os = std::make_unique<juce::dsp::Oversampling<double>> ((size_t) numCh, (size_t) i,
                juce::dsp::Oversampling<double>::filterHalfBandPolyphaseIIR, true, true);
            set.os->initProcessing ((size_t) maxBlock);
        }
        for (int m = 0; m < 3; ++m) { measureLinear (set, m, hs[i][m]); set.tau[m] = pa::dcDelay (hs[i][m]); }
        set.eng.setModel (model);
        set.eng.update (s, true);
        set.eng.prepare (fs, set.factor, maxAhead);
    }
    // ---- output alignment, the same total latency in every OVERSAMPLE x FILTER setting
    // ANALOG: a fractional delay lands the low-frequency delay on whole samples (88.2 kHz and up; below: rounded)
    // LINEAR: a correction FIR removes the filters' phase (1 ms before the main tap, 0.3 ms after)
    const int dfd = fs >= 60000.0 ? 8 : 0;
    const int P = (int) std::lround (1.0e-3 * fs), R = (int) std::lround (0.3e-3 * fs);
    int Ta = 0, Tl = 0;
    for (int i = 0; i < 4; ++i)
        for (int m = 0; m < 3; ++m)
        {
            if (sets[(size_t) i].factor > maxFactor) continue;
            const double t = sets[(size_t) i].tau[m];
            Ta = juce::jmax (Ta, dfd > 0 ? (int) std::ceil (t - 1e-9) + dfd : (int) std::lround (t));
            Tl = juce::jmax (Tl, P + (int) std::ceil (t - 1e-9));
        }
    int maxLen = 1;
    for (int i = 0; i < 4; ++i)
    {
        auto& set = sets[(size_t) i];
        if (set.factor > maxFactor) continue;
        for (int m = 0; m < 3; ++m)
        {
            const double t = set.tau[m];
            if (dfd > 0)
            {
                const double d = Ta - t; const int fl = (int) std::floor (d + 1e-9);
                set.fir[0][m] = pa::fracDelayFir (dfd, juce::jmax (0.0, d - fl)); set.pad[0][m] = fl - dfd;
            }
            else { set.fir[0][m] = { 1.0 }; set.pad[0][m] = Ta - (int) std::lround (t); }
            set.fir[1][m] = pa::linearPhaseFir (hs[i][m], t, P, R);
            set.pad[1][m] = Tl - (P + (int) std::ceil (t - 1e-9));
            maxLen = juce::jmax (maxLen, (int) set.fir[1][m].size(), (int) set.fir[0][m].size());
        }
    }
    fixedLat[0] = Ta + 1; fixedLat[1] = Tl + 1;       // + the Pre's linear-phase roll-off (one sample)
    for (int c = 0; c < 2; ++c)
    {
        outFir[c].prepare (maxLen + 1);
        padDelay[c].prepare (juce::jmax (Ta, Tl) + 2);
        bypassDelay[c].prepare (juce::jmax (fixedLat[0], fixedLat[1]) + maxAhead + 2);
    }
    work.setSize (2, maxBlock, false, false, true);
    dryBuf.setSize (2, maxBlock, false, false, true);
    bypassMix.reset (fs, 0.01);
    bypassMix.setCurrentAndTargetValue (p[pPower]->load() > 0.5f ? 1.0 : 0.0);
    lastAheadIdx = -1; lastPhase = -1;
    updateLatency();

    const int f = wantedFactor (isNonRealtime());
    for (int i = 0; i < 4; ++i) if (kFactors[i] == f) activeSet = i;
    activeFactor = f;
}

void FreeLA2Processor::updateLatency()
{
    const int idx = juce::jlimit (0, 4, juce::roundToInt (p[pAhead]->load()));
    const int ph = juce::jlimit (0, 1, juce::roundToInt (p[pPhase]->load()));
    if (idx == lastAheadIdx && ph == lastPhase) return;
    lastAheadIdx = idx; lastPhase = ph;
    const int total = fixedLat[ph] + (int) std::lround (kAheadMs[idx] * 1e-3 * fs);
    latencyNow = total;
    setLatencySamples (total);
}

void FreeLA2Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nc = juce::jmin (numCh, buffer.getNumChannels());
    if (n == 0 || nc == 0) return;
    if (n > work.getNumSamples())
    {
        work.setSize (2, n, false, false, true); dryBuf.setSize (2, n, false, false, true);   // safety: host sent a bigger block
    }

    updateLatency();
    const bool offline = isNonRealtime();
    const int f = wantedFactor (offline);
    if (f != kFactors[activeSet])
    {
        for (int i = 0; i < 4; ++i) if (kFactors[i] == f) activeSet = i;
        auto& set = sets[(size_t) activeSet];
        if (set.os) set.os->reset();
        set.eng.reset();
        for (auto& d : padDelay) d.clear();
        for (auto& f : outFir) f.clear();
    }
    activeFactor = f;
    auto& set = sets[(size_t) activeSet];

    const auto s = readSettings();
    set.eng.update (s);
    filtersOn = set.eng.filtersActive();
    const bool on = p[pPower]->load() > 0.5f;
    bypassMix.setTargetValue (on ? 1.0 : 0.0);

    for (int c = 0; c < nc; ++c)
    {
        const float* in = buffer.getReadPointer (c);
        double* w = work.getWritePointer (c); double* d = dryBuf.getWritePointer (c);
        for (int i = 0; i < n; ++i) { w[i] = in[i]; d[i] = in[i]; }
    }

    // ---- oversampled engine (Pre > Comp > Clipper, band-limiting and Mix inside)
    juce::dsp::AudioBlock<double> block (work.getArrayOfWritePointers(), (size_t) nc, (size_t) n);
    juce::dsp::AudioBlock<double> up = set.os ? set.os->processSamplesUp (block) : block;
    const int nu = (int) up.getNumSamples();
    set.eng.process (up.getChannelPointer (0), nc > 1 ? up.getChannelPointer (1) : nullptr, nu);
    if (set.os) set.os->processSamplesDown (block);

    // ---- base rate: alignment FIR (ANALOG or LINEAR) + pad to the fixed latency, click-free power switch, meters
    const int mode = juce::jlimit (0, 2, s.filterMode);
    const int ph = juce::jlimit (0, 1, lastPhase);
    const auto& fir = set.fir[ph][mode];
    const int pad = juce::jmax (0, set.pad[ph][mode]);
    const int total = latencyNow.load();
    double sumSq = 0;
    for (int i = 0; i < n; ++i)
    {
        const double bm = bypassMix.getNextValue();
        for (int c = 0; c < nc; ++c)
        {
            double* w = work.getWritePointer (c);
            const double wet = padDelay[c].process (outFir[c].process (fir, w[i]), pad);
            const double dry = bypassDelay[c].process (dryBuf.getSample (c, i), total);
            w[i] = bm * wet + (1.0 - bm) * dry;
            sumSq += w[i] * w[i];
        }
    }
    for (int c = 0; c < nc; ++c)
    {
        float* out = buffer.getWritePointer (c); const double* w = work.getReadPointer (c);
        for (int i = 0; i < n; ++i) out[i] = (float) w[i];
    }
    for (int c = nc; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);

    const double gr = set.eng.takeGrMax(), cl = set.eng.takeClipMax();
    grMeter = on ? (float) gr : 0.f;
    clipLevel = on ? (float) cl : 0.f;
    outRms = (float) std::sqrt (sumSq / (double) (n * nc));
}

void FreeLA2Processor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void FreeLA2Processor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* FreeLA2Processor::createEditor() { return new FreeLA2Editor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FreeLA2Processor(); }
