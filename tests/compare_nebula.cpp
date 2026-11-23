// Runs the FreeLA2 plug-in (the same processor as the VST3/AU) over a list of jobs, back to back in ONE instance,
// the way REAPER rendered the Nebula passes (state carries over from render to render).
// Not built on GitHub. Driven by fit/compare_nebula.py.
//
// jobs file, one job per line:
//   <in.f32> <out.f32> <frames> stages=<1 Pre | 2 Leveler | 4 Clipper> <param id>=<value> ...
// in/out: raw interleaved stereo float32. The output is latency-compensated (same length as the input).
// The first line may be:  global oslive=<0..3> filter=<0..2> rate=<Hz>   (set before prepareToPlay)
#include "../Source/PluginProcessor.h"
#include <fstream>
#include <iostream>
#include <sstream>

static void setParam (FreeLA2Processor& p, const juce::String& id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    if (prm == nullptr) { std::cerr << "unknown parameter " << id << std::endl; std::exit (2); }
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

int main (int argc, char** argv)
{
    if (argc < 2) { std::cerr << "usage: FreeLA2Compare jobs.txt" << std::endl; return 1; }
    juce::ScopedJuceInitialiser_GUI init;
    std::ifstream jf (argv[1]);
    std::string line;
    std::vector<std::string> lines;
    while (std::getline (jf, line)) if (! line.empty()) lines.push_back (line);

    double rate = 96000; const int block = 512;
    FreeLA2Processor p;
    setParam (p, ID::rendermode, 2.f);      // Never: always the live oversampling setting
    setParam (p, ID::oslive, 0.f);
    setParam (p, ID::filter, 0.f);
    size_t first = 0;
    if (! lines.empty() && lines[0].rfind ("global", 0) == 0)
    {
        std::istringstream in (lines[0]); std::string tok; in >> tok;
        while (in >> tok)
        {
            const auto eq = tok.find ('='); const auto k = tok.substr (0, eq); const float v = std::stof (tok.substr (eq + 1));
            if (k == "rate") rate = v; else setParam (p, k, v);
        }
        first = 1;
    }
    p.setPlayConfigDetails (2, 2, rate, block);
    p.prepareToPlay (rate, block);
    const int lat = p.getLatencySamples();
    std::cout << "latency " << lat << " samples, factor " << p.activeFactor.load() << std::endl;

    juce::AudioBuffer<float> b (2, block); juce::MidiBuffer mb;
    for (size_t j = first; j < lines.size(); ++j)
    {
        std::istringstream in (lines[j]);
        std::string inPath, outPath; long frames = 0;
        in >> inPath >> outPath >> frames;
        std::string tok;
        while (in >> tok)
        {
            const auto eq = tok.find ('='); const auto k = tok.substr (0, eq); const float v = std::stof (tok.substr (eq + 1));
            if (k == "stages") p.testStages = (int) v; else setParam (p, k, v);
        }
        std::vector<float> x ((size_t) frames * 2);
        { std::ifstream f (inPath, std::ios::binary); f.read ((char*) x.data(), (std::streamsize) (x.size() * sizeof (float))); }
        std::vector<float> y ((size_t) frames * 2, 0.f);
        const long total = frames + lat;
        long outPos = -lat;
        for (long pos = 0; pos < total; pos += block)
        {
            const int n = (int) std::min ((long) block, total - pos);
            b.setSize (2, n, false, false, true);
            for (int i = 0; i < n; ++i)
            {
                const long k = pos + i;
                b.setSample (0, i, k < frames ? x[(size_t) (2 * k)] : 0.f);
                b.setSample (1, i, k < frames ? x[(size_t) (2 * k + 1)] : 0.f);
            }
            p.processBlock (b, mb);
            for (int i = 0; i < n; ++i, ++outPos)
                if (outPos >= 0 && outPos < frames) { y[(size_t) (2 * outPos)] = b.getSample (0, i); y[(size_t) (2 * outPos + 1)] = b.getSample (1, i); }
        }
        std::ofstream f (outPath, std::ios::binary); f.write ((const char*) y.data(), (std::streamsize) (y.size() * sizeof (float)));
    }
    std::cout << "done " << (lines.size() - first) << " jobs" << std::endl;
    return 0;
}
