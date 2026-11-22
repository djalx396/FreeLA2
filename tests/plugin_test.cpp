// Local test of the FreeLA2 plug-in (Linux): processing, latency, oversampling, band-limit modes, bypass,
// state, mono, engine equivalence, and a GUI snapshot. Not built on GitHub.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/ModelData.h"
#include <iostream>

static void setParam (FreeLA2Processor& p, const juce::String& id, float v)
{
    auto* prm = p.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}
static std::vector<float> run (FreeLA2Processor& p, const std::vector<float>& inL, const std::vector<float>& inR, int block, int nch,
                               std::vector<float>* outR = nullptr, float* grMax = nullptr)
{
    std::vector<float> out (inL.size());
    if (outR) outR->assign (inL.size(), 0.f);
    juce::AudioBuffer<float> b (nch, block); juce::MidiBuffer mb;
    float gm = 0;
    for (size_t pos = 0; pos < inL.size(); pos += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, inL.size() - pos);
        b.setSize (nch, n, false, false, true);
        for (int i = 0; i < n; ++i) { b.setSample (0, i, inL[pos + (size_t) i]); if (nch > 1) b.setSample (1, i, inR[pos + (size_t) i]); }
        p.processBlock (b, mb);
        for (int i = 0; i < n; ++i) { out[pos + (size_t) i] = b.getSample (0, i); if (outR && nch > 1) (*outR)[pos + (size_t) i] = b.getSample (1, i); }
        gm = std::max (gm, p.grMeter.load());
    }
    if (grMax) *grMax = gm;
    return out;
}
static double rmsDb (const std::vector<float>& x, size_t a, size_t b)
{
    double s = 0; for (size_t i = a; i < b; ++i) s += (double) x[i] * x[i];
    return 10.0 * std::log10 (s / (double) (b - a) + 1e-30);
}
static bool finite (const std::vector<float>& x) { for (float v : x) if (! std::isfinite (v)) return false; return true; }

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    int fails = 0;
    auto check = [&] (bool ok, const juce::String& what) { std::cout << (ok ? "  ok   " : "  FAIL ") << what << std::endl; if (! ok) ++fails; };
    const double fs = 96000; const int block = 512;
    const int N = (int) (3.0 * fs);
    std::vector<float> sL ((size_t) N), sR ((size_t) N), pinkL ((size_t) N), pinkR ((size_t) N);
    juce::Random rnd (7); double b0 = 0, b1 = 0, b2 = 0;
    for (int i = 0; i < N; ++i)
    {
        const double t = i / fs; const double env = t < 1.0 ? 0.0316 : 0.316;   // -30 dBFS, then -10 dBFS
        sL[(size_t) i] = (float) (env * std::sin (2 * juce::MathConstants<double>::pi * 1000 * t)); sR[(size_t) i] = sL[(size_t) i];
        const double w = rnd.nextDouble() * 2 - 1; b0 = 0.99765 * b0 + w * 0.0990460; b1 = 0.96300 * b1 + w * 0.2965164; b2 = 0.57000 * b2 + w * 1.0526913;
        pinkL[(size_t) i] = (float) (0.05 * (b0 + b1 + b2 + w * 0.1848)); pinkR[(size_t) i] = (float) (0.05 * (b0 - b1 + b2 + w * 0.17));
    }

    std::cout << "FreeLA2 " << FreeLA2Processor::versionString() << std::endl;
    // 1) every oversampling factor x band-limit mode: finite output, same latency, level consistent
    {
        FreeLA2Processor p; p.setPlayConfigDetails (2, 2, fs, block); p.prepareToPlay (fs, block);
        const int lat0 = p.getLatencySamples();
        std::cout << "  latency " << lat0 << " samples" << std::endl;
        double ref = 0; bool first = true;
        for (int os = 0; os < 4; ++os)
            for (int fm = 0; fm < 3; ++fm)
            {
                setParam (p, ID::oslive, (float) os); setParam (p, ID::filter, (float) fm);
                float gr = 0; auto y = run (p, sL, sR, block, 2, nullptr, &gr);
                const double lv = rmsDb (y, (size_t) (2.0 * fs), (size_t) N);
                if (first) { ref = lv; first = false; }
                check (finite (y) && std::abs (lv - ref) < 0.3 && p.getLatencySamples() == lat0,
                       juce::String ("OS ") + juce::String (1 << os) + "x, filter " + juce::String (fm) + ": level " + juce::String (lv, 2) + " dB, GR " + juce::String (gr, 2)
                       + " dB, latency " + juce::String (p.getLatencySamples()));
            }
    }
    // 2) plug-in == engine (OS Off, filter OFF), after the reported latency
    {
        FreeLA2Processor p; p.setPlayConfigDetails (2, 2, fs, block);
        setParam (p, ID::oslive, 0); setParam (p, ID::filter, 0); setParam (p, ID::reduction, 60.f); setParam (p, ID::drive, 12.f); setParam (p, ID::clipdrive, 20.f);
        p.prepareToPlay (fs, block);
        auto y = run (p, pinkL, pinkR, block, 2);
        la2::Engine e; e.setModel (la2::makeModel()); la2::Settings s; s.reduction = 60; s.drive = 12; s.clipIn = 20; s.filterMode = 0; s.mix = 1;
        e.update (s, true); e.prepare (fs, 1, (int) std::ceil (6e-3 * fs) + 1);
        std::vector<double> L (pinkL.begin(), pinkL.end()), R (pinkR.begin(), pinkR.end());
        e.process (L.data(), R.data(), N);
        const int lat = p.getLatencySamples() - 1;           // the engine itself already contains the Pre's one-sample delay
        double d = 0, r = 0;
        for (int i = 48000; i < N - lat; ++i) { const double v = y[(size_t) (i + lat)] - L[(size_t) i]; d += v * v; r += L[(size_t) i] * L[(size_t) i]; }
        check (10 * std::log10 (d / r + 1e-30) < -100, "plug-in output equals the engine: " + juce::String (10 * std::log10 (d / r + 1e-30), 1) + " dB");
    }
    // 3) power off = input delayed by the latency
    {
        FreeLA2Processor p; p.setPlayConfigDetails (2, 2, fs, block); setParam (p, ID::power, 0.f); p.prepareToPlay (fs, block);
        auto y = run (p, pinkL, pinkR, block, 2); const int lat = p.getLatencySamples();
        double d = 0; for (int i = 48000; i < N - lat; ++i) { const double v = y[(size_t) (i + lat)] - pinkL[(size_t) i]; d += v * v; }
        check (d < 1e-12, "power off is a clean, latency-matched bypass");
    }
    // 4) AHEAD changes the reported latency; Mix 0 = dry (aligned)
    {
        FreeLA2Processor p; p.setPlayConfigDetails (2, 2, fs, block); p.prepareToPlay (fs, block);
        const int l0 = p.getLatencySamples(); setParam (p, ID::ahead, 2.f); juce::AudioBuffer<float> b (2, 64); juce::MidiBuffer mb; b.clear(); p.processBlock (b, mb);
        check (p.getLatencySamples() == l0 + (int) std::lround (3e-3 * fs), "AHEAD 3 ms adds " + juce::String (p.getLatencySamples() - l0) + " samples");
        setParam (p, ID::ahead, 0.f); setParam (p, ID::mix, 0.f); setParam (p, ID::filter, 0.f); setParam (p, ID::oslive, 0.f);
        auto y = run (p, pinkL, pinkR, block, 2); const int lat = p.getLatencySamples();
        double d = 0, r = 0; for (int i = 48000; i < N - lat; ++i) { const double v = y[(size_t) (i + lat)] - pinkL[(size_t) i]; d += v * v; r += (double) pinkL[(size_t) i] * pinkL[(size_t) i]; }
        check (10 * std::log10 (d / r + 1e-30) < -100, "Mix 0 (filter OFF) = dry signal: " + juce::String (10 * std::log10 (d / r + 1e-30), 1) + " dB");
    }
    // 5) mono layout, 44.1 and 192 kHz, state round trip
    {
        FreeLA2Processor p; p.setPlayConfigDetails (1, 1, 44100, block); p.prepareToPlay (44100, block);
        auto y = run (p, pinkL, pinkR, block, 1); check (finite (y), "mono at 44.1 kHz");
        FreeLA2Processor q; q.setPlayConfigDetails (2, 2, 192000, block); setParam (q, ID::oslive, 3.f); q.prepareToPlay (192000, block);
        auto z = run (q, pinkL, pinkR, block, 2); check (finite (z) && q.activeFactor.load() == 4, "192 kHz: 8x falls back to 4x");
        setParam (p, ID::reduction, 77.f); setParam (p, ID::limit, 1.f); juce::MemoryBlock mbk; p.getStateInformation (mbk);
        FreeLA2Processor r2; r2.setStateInformation (mbk.getData(), (int) mbk.getSize());
        check (std::abs (r2.apvts.getRawParameterValue (ID::reduction)->load() - 77.f) < 0.01f && r2.apvts.getRawParameterValue (ID::limit)->load() > 0.5f, "state saves and reloads");
    }
    // 6) GUI snapshot
    {
        FreeLA2Processor p; p.setPlayConfigDetails (2, 2, fs, block); p.prepareToPlay (fs, block);
        setParam (p, ID::reduction, 55.f); run (p, sL, sR, block, 2);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        for (int k = 0; k < 40; ++k) { juce::MessageManager::getInstance()->runDispatchLoopUntil (10); }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f ("/tmp/freela2_gui.png"); f.deleteFile();
        juce::FileOutputStream os (f); juce::PNGImageFormat png; png.writeImageToStream (img, os);
        check (img.getWidth() == 1120 && img.getHeight() == 620, "GUI snapshot written to /tmp/freela2_gui.png");
        for (int fin = 1; fin < 3; ++fin)
        {
            setParam (p, ID::finish, (float) fin);
            for (int k = 0; k < 10; ++k) juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
            auto im2 = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::File f2 ("/tmp/freela2_gui_" + juce::String (fin) + ".png"); f2.deleteFile(); juce::FileOutputStream o2 (f2); png.writeImageToStream (im2, o2);
        }
    }
    std::cout << (fails == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << std::endl;
    return fails == 0 ? 0 : 1;
}
