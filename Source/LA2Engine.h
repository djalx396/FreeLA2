// FreeLA2 engine: Pre > Comp (opto leveler) > Clipper, Hypersonic band-limiting, Mix.
// Plain C++ (no JUCE). Runs at the internal (oversampled) rate; the compressor detector
// runs at the session rate (one update every `factor` internal samples).
//
// Band-limiting (OFF / CLEAN / AIR) comes from FreeBandLimit.h (Airwindows Hypersonic sections, MIT licence).
#pragma once
#include "FreeBandLimit.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include <cstring>

namespace la2
{
constexpr double kPi = 3.14159265358979323846;

// ------------------------------------------------------------------ model layout (flat array)
namespace M
{
    // Pre
    constexpr int lfF = 0, lfQ = 1, hfF = 2, hfQ = 3, k2 = 4, k3 = 5, h2ShelfF = 6, h2ShelfDb = 7, h2HpF = 8,
                  envRelMs = 9, g2Tab = 10;          // 24 values: NORMAL H2 trim (dB) at -40..+6 dBFS, step 2
    constexpr int g2N = 24;
    // LF: two analog sections (fz qz fp qp each): section 1 = [lfF, lfQ, 34, 35], section 2 = [36 .. 39]
    constexpr int lf1fp = 34, lf1qp = 35, lf2fz = 36, lf2qz = 37, lf2fp = 38, lf2qp = 39;
    constexpr int preTrimDb = 43, preDelayUs = 44;
    constexpr int ripHpF = 45;      // ripple high-pass (Hz): the cell ripple follows the waveform, not envelope jumps (0 = off) // fixed gain and sub-sample delay of the Nebula Pre
    // Clipper
    constexpr int clipC = 40, clipN = 41, clipLeak = 42;
    // EMPHASIS: 5 points (0, .25, .5, .75, 1) x (fc Hz, low-band weight, low-band attack ms)
    constexpr int emph = 48;
    // compressor program blocks
    constexpr int prog0 = 64, progSize = 830;
    namespace P
    {
        constexpr int tauM = 0, tauP = 1, beta = 2, w1 = 3, w2 = 4, ta1 = 5, E = 6, pexp = 7, ta2 = 8, ta3 = 9,
                      tr1 = 10, tr2 = 11, tr3 = 12, detOff = 13, cap2 = 14, cap3 = 15, lfTr = 16, qRel = 17, d3a = 18, d3p = 19,
                      u0 = 20, du = 21, nu = 22, curves = 23;   // 3 curves x nu
        constexpr int nuMax = 201;
        constexpr int theta = 23 + 3 * nuMax;                   // 101 values, r = 0..1 step 0.01
        constexpr int cpos = theta + 101;                       // 101 values, 0..2 (curve blend coordinate)
        constexpr int d3cap = cpos + 101, d3fc = cpos + 102;    // gain-cell ripple: saturation GR, cell low-pass (Hz)
    }
    constexpr int hfTab = prog0 + 2 * progSize;  // fitted HF biquads (b0 b1 b2 a1 a2) at 44.1, 48, 88.2, 96 kHz
    constexpr int size = hfTab + 24;
}

// ------------------------------------------------------------------ settings
struct Settings
{
    bool   preOn = true, compOn = true, clipOn = true;
    bool   pushed = false;      // Pre program
    double drive = 0.0;         // Pre DRIVE, -30..+30 (dB of 2nd harmonic)
    double levelDb = 0.0;       // Pre OUTPUT = Leveler INPUT (one gain; 0 = Nebula as loaded)
    bool   limit = false;       // Comp program
    double reduction = 0.0;     // PEAK REDUCTION 0..100
    double emphasis = 0.0;      // -100 (FASTER ATTACK) .. +100 (BASS SLOW ATT)
    double gainDb = 0.0;        // Comp GAIN 0..25
    double aheadMs = 0.0;       // look-ahead
    double clipIn = 0.0;        // Clipper DRIVE 0..40 dB
    double clipOutDb = 0.0;     // Clipper OUTPUT 0..10 dB
    double mix = 1.0;           // Leveler wet/dry 0..1 (0 = Pre straight into the Clipper)
    int    filterMode = 1;      // 0 OFF, 1 CLEAN, 2 AIR (active only when the internal rate is >= 88.2 kHz)
    int    filtersForce = -1;   // -1 auto, 0 off, 1 on regardless of rate (tests only)
};

// ------------------------------------------------------------------ small DSP blocks
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, s1 = 0, s2 = 0;
    inline double process (double x) noexcept
    {
        const double y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2;
        s2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { s1 = s2 = 0; }
    void setIdentity() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
    void lowpass (double f, double q, double fs)
    {
        const double K = std::tan (kPi * std::min (f / fs, 0.49)), n = 1.0 / (1.0 + K / q + K * K);
        b0 = K * K * n; b1 = 2 * b0; b2 = b0; a1 = 2 * (K * K - 1) * n; a2 = (1 - K / q + K * K) * n;
    }
    void highpass (double f, double q, double fs)
    {
        const double K = std::tan (kPi * std::min (f / fs, 0.49)), n = 1.0 / (1.0 + K / q + K * K);
        b0 = n; b1 = -2 * n; b2 = n; a1 = 2 * (K * K - 1) * n; a2 = (1 - K / q + K * K) * n;
    }
    // H(s) = (s^2 + s wz/qz + wz^2) / (s^2 + s wp/qp + wp^2), bilinear prewarped at fp
    void analog2 (double fz, double qz, double fp, double qp, double fs)
    {
        const double wz = 2 * kPi * fz, wp = 2 * kPi * fp, c = wp / std::tan (std::min (wp / (2 * fs), 1.5));
        const double a0 = c * c + c * wp / qp + wp * wp;
        b0 = (c * c + c * wz / qz + wz * wz) / a0; b1 = (2 * wz * wz - 2 * c * c) / a0; b2 = (c * c - c * wz / qz + wz * wz) / a0;
        a1 = (2 * wp * wp - 2 * c * c) / a0; a2 = (c * c - c * wp / qp + wp * wp) / a0;
    }
    void lowShelf1 (double f, double gainDb, double fs)   // first-order low shelf (bilinear)
    {
        const double A = std::pow (10.0, gainDb / 20.0), K = std::tan (kPi * std::min (f / fs, 0.49));
        // H(s) = (s + A w) / (s + w)
        const double n = 1.0 / (1.0 + K);
        b0 = (1.0 + A * K) * n; b1 = (A * K - 1.0) * n; b2 = 0; a1 = (K - 1.0) * n; a2 = 0;
    }
};

struct OnePole   // y += c (x - y)
{
    double c = 1, y = 0;
    void setMs (double ms, double fs) { c = ms <= 0 ? 1.0 : 1.0 - std::exp (-1.0 / (ms * 1e-3 * fs)); }
    void setHzLP (double f, double fs) { c = 1.0 - std::exp (-2.0 * kPi * f / fs); }
    inline double process (double x) noexcept { y += c * (x - y); return y; }
    void reset() { y = 0; }
};

struct Delay
{
    std::vector<double> buf; int w = 0, d = 0;
    void prepare (int maxD) { buf.assign ((size_t) std::max (1, maxD + 1), 0.0); w = 0; d = 0; }
    void setDelay (int dd) { d = std::max (0, std::min (dd, (int) buf.size() - 1)); }
    inline double process (double x) noexcept
    {
        if (d == 0) return x;
        buf[(size_t) w] = x;
        int r = w - d; if (r < 0) r += (int) buf.size();
        w = (w + 1 == (int) buf.size()) ? 0 : w + 1;
        return buf[(size_t) r];
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0); w = 0; }
};

// short FIR (2D+1 taps), least-squares fit to a target magnitude with delay D + frac samples (near linear phase)
struct SymFir
{
    std::vector<double> h, buf; int D = 0, w = 0;
    template <class Target>
    void design (int delay, double frac, double fs, double fmax, Target target)
    {
        D = std::max (1, delay); const int N = 2 * D + 1;
        std::vector<double> A ((size_t) (N * N), 0.0), b ((size_t) N, 0.0), row ((size_t) N);
        auto addRow = [&] (double y, double wgt)
        {
            for (int i = 0; i < N; ++i) { b[(size_t) i] += wgt * row[(size_t) i] * y; for (int j = 0; j < N; ++j) A[(size_t) (i * N + j)] += wgt * row[(size_t) i] * row[(size_t) j]; }
        };
        const int G = 400; const double tau = D + frac;
        for (int g = 0; g <= G; ++g)
        {
            const double f = fmax * g / G, wv = 2 * kPi * f / fs, t = target (f), wgt = g == 0 ? 1000.0 : 1.0;
            for (int n = 0; n < N; ++n) row[(size_t) n] = std::cos (wv * n);
            addRow (t * std::cos (wv * tau), wgt);
            if (g == 0) continue;
            for (int n = 0; n < N; ++n) row[(size_t) n] = -std::sin (wv * n);
            addRow (-t * std::sin (wv * tau), wgt);
        }
        for (int i = 0; i < N; ++i) A[(size_t) (i * N + i)] += 1e-9;
        for (int i = 0; i < N; ++i)                                  // Gaussian elimination with pivoting
        {
            int piv = i; for (int r2 = i + 1; r2 < N; ++r2) if (std::abs (A[(size_t) (r2 * N + i)]) > std::abs (A[(size_t) (piv * N + i)])) piv = r2;
            for (int j = 0; j < N; ++j) std::swap (A[(size_t) (i * N + j)], A[(size_t) (piv * N + j)]);
            std::swap (b[(size_t) i], b[(size_t) piv]);
            for (int r2 = i + 1; r2 < N; ++r2)
            {
                const double f = A[(size_t) (r2 * N + i)] / A[(size_t) (i * N + i)];
                for (int j = i; j < N; ++j) A[(size_t) (r2 * N + j)] -= f * A[(size_t) (i * N + j)];
                b[(size_t) r2] -= f * b[(size_t) i];
            }
        }
        h.assign ((size_t) N, 0.0);
        for (int i = N - 1; i >= 0; --i) { double v = b[(size_t) i]; for (int j = i + 1; j < N; ++j) v -= A[(size_t) (i * N + j)] * h[(size_t) j]; h[(size_t) i] = v / A[(size_t) (i * N + i)]; }
        buf.assign (h.size(), 0.0); w = 0;
    }
    inline double process (double x) noexcept
    {
        const int N = (int) h.size();
        buf[(size_t) w] = x;
        double y = 0; int r = w;
        for (int k = 0; k < N; ++k) { y += h[(size_t) k] * buf[(size_t) r]; r = (r == 0) ? N - 1 : r - 1; }
        w = (w + 1 == N) ? 0 : w + 1;
        return y;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0); w = 0; }
};

// band-limiting layouts: slots 0 before Pre, 1 after Pre, 2 after Comp, 3 after Clipper
inline const char* const* layoutSpecs (int mode)
{
    static const char* const clean[4] = { "", "HYP", "HYP", "HYP" };
    static const char* const air[4]   = { "B4:D", "B4:C", "B4:B", "B4:A HYP" };
    return mode == 1 ? clean : (mode == 2 ? air : nullptr);
}

// ------------------------------------------------------------------ model wrapper
class Model
{
public:
    std::vector<double> v;
    Model() : v ((size_t) M::size, 0.0) {}
    Model (const double* d, int n) : v ((size_t) M::size, 0.0) { std::memcpy (v.data(), d, sizeof (double) * (size_t) std::min (n, M::size)); }
    double operator[] (int i) const { return v[(size_t) i]; }
    const double* prog (bool limit) const { return v.data() + M::prog0 + (limit ? M::progSize : 0); }
};

// static GR (dB) for detector level d (dB) and reduction r (0..1)
inline double staticGR (const double* P, double d, double r)
{
    namespace I = M::P;
    const double u0 = P[I::u0], du = P[I::du]; const int nu = (int) P[I::nu];
    const double rr = std::max (0.0, std::min (1.0, r)) * 100.0;
    const int ri = std::min (99, (int) rr); const double rf = rr - ri;
    const double th = P[I::theta + ri] * (1 - rf) + P[I::theta + ri + 1] * rf;
    const double cp = P[I::cpos + ri] * (1 - rf) + P[I::cpos + ri + 1] * rf;
    const int ci = std::min (1, (int) cp); const double cf = cp - ci;
    auto curve = [&] (int k, double u)
    {
        const double x = (u - u0) / du;
        const double* c = P + I::curves + k * I::nuMax;
        if (x <= 0) return c[0];
        if (x >= nu - 1) return c[nu - 1] + (c[nu - 1] - c[nu - 2]) * (x - (nu - 1));   // linear extrapolation
        const int i = (int) x; const double f = x - i;
        return c[i] * (1 - f) + c[i + 1] * f;
    };
    const double u = d - th;
    return std::max (0.0, curve (ci, u) * (1 - cf) + curve (ci + 1, u) * cf);
}

// ------------------------------------------------------------------ engine
class Engine
{
public:
    void setModel (const Model& m) { model = m; }

    void prepare (double baseRate, int osFactor, int maxAheadBase = 600)
    {
        fs = baseRate; factor = std::max (1, osFactor); fo = fs * factor;
        filtersAuto = fo >= 88000.0;
        maxAhead = maxAheadBase;
        for (int c = 0; c < 2; ++c)
        {
            aheadDel[c].prepare (maxAhead * factor + 2);
            preByp[c].prepare (factor + 2); preByp[c].setDelay (factor);   // Pre switched off: keep its one-sample latency
        }
        prepared = true;
        lastLayout = -2;
        designFixed();
        update (set, true);
        reset();
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            lf[c].reset(); lfSh[c].reset(); hfFir[c].reset(); h2shelf[c].reset(); h2hp1[c].reset(); h2hp2[c].reset(); env[c] = 0;
            bl[c].reset(); aheadDel[c].reset(); preByp[c].reset(); g2Cache[c] = 1; g2Count[c] = 0;
        }
        ripEnv = 0; ripLp.reset(); ripHp1.reset(); ripHp2.reset(); kr = 0;
        msH = msL = pkH = pkL = 0; lfSm = 0; split.reset(); G1 = G2 = G3 = 0;
        gPrev = gNow = 1.0; tick = 0; grOut = 0;
    }

    // call when settings change (cheap); `force` redesigns everything
    void update (const Settings& s, bool force = false)
    {
        const bool limitChanged = s.limit != set.limit;
        set = s;
        const bool fOn = s.filtersForce < 0 ? filtersAuto : s.filtersForce > 0;
        const int lay = fOn ? s.filterMode : 0;
        if (force || lay != lastLayout) { buildFilters (lay); lastLayout = lay; }
        // Pre
        tLevel = std::pow (10.0, s.levelDb / 20.0);
        tK2 = model[M::k2] * std::pow (10.0, s.drive / 20.0);
        k3 = model[M::k3];
        // Comp
        P = model.prog (s.limit);
        tGOut = std::pow (10.0, s.gainDb / 20.0);
        r = std::max (0.0, std::min (1.0, s.reduction / 100.0));
        updateDetector (force || limitChanged);
        // Clipper
        tCg = std::pow (10.0, s.clipIn / 20.0);
        tCOut = std::pow (10.0, s.clipOutDb / 20.0);
        tMix = std::max (0.0, std::min (1.0, s.mix));
        if (force) { level = tLevel; k2 = tK2; gOut = tGOut; cg = tCg; cOut = tCOut; mix = tMix; }
        // AHEAD (whole session-rate samples)
        aheadBase = std::max (0, std::min (maxAhead, (int) std::lround (s.aheadMs * 1e-3 * fs)));
        for (int c = 0; c < 2; ++c) aheadDel[c].setDelay (aheadBase * factor);
    }

    // in-place, internal rate, n samples; nch 1 or 2
    void process (double* L, double* R, int n, double* grTrace = nullptr)
    {
        const int nch = R ? 2 : 1;
        double* ch[2] = { L, R };
        const double c4 = model[M::clipC], cn = model[M::clipN], invC = 1.0 / c4, leak = model[M::clipLeak];
        (void) cn;
        const double envRel = envRelC;
        const double* g2t = model.v.data() + M::g2Tab;
        for (int i = 0; i < n; ++i)
        {
            double x[2];
            for (int c = 0; c < nch; ++c) x[c] = ch[c][i];
            // parameter smoothing (about 20 ms)
            level += sm * (tLevel - level); k2 += sm * (tK2 - k2); gOut += sm * (tGOut - gOut);
            cg += sm * (tCg - cg); cOut += sm * (tCOut - cOut); mix += sm * (tMix - mix);
            const double invCg = 1.0 / cg;
            // ---- slot 0, Pre
            for (int c = 0; c < nch; ++c)
            {
                double v = bl[c].process (0, x[c]);
                if (set.preOn)
                {
                    v = hfFir[c].process (lfSh[c].process (lf[c].process (v))) * preTrim;
                    // 2nd harmonic: x^2 -> shelf -> high-pass (no DC / thump), scaled by DRIVE and (NORMAL) level
                    double g2 = 1.0;
                    if (! set.pushed)
                    {
                        const double a = std::abs (v);
                        env[c] = a > env[c] ? a : env[c] * envRel;
                        if ((g2Count[c]++ & 15) == 0)          // the level-dependent trim changes slowly: update every 16 samples
                        {
                            const double lv = 20.0 * std::log10 (env[c] + 1e-12);
                            const double t = (lv + 40.0) / 2.0;
                            double gdb;
                            if (t <= 0) gdb = g2t[0];
                            else if (t >= M::g2N - 1) gdb = g2t[M::g2N - 1];
                            else { const int k = (int) t; const double f = t - k; gdb = g2t[k] * (1 - f) + g2t[k + 1] * f; }
                            g2Cache[c] = std::pow (10.0, gdb / 20.0);
                        }
                        g2 = g2Cache[c];
                    }
                    double h2 = h2shelf[c].process (v * v);
                    h2 = h2hp2[c].process (h2hp1[c].process (h2));
                    v = v + k2 * g2 * h2 + k3 * v * v * v;
                }
                else v = preByp[c].process (v);
                x[c] = bl[c].process (1, v * level);          // Pre OUTPUT = Leveler INPUT
            }
            // ---- Comp: detector at the session rate, gain ramped across the internal samples
            if (set.compOn)
            {
                const double* u = x;
                if (tick == 0)
                {
                    const double sc = nch == 2 ? 0.5 * (u[0] + u[1]) : u[0];
                    detect (sc);
                    gPrev = gNow;
                    gNow = std::pow (10.0, -grOut / 20.0);
                    if (d3a > 0) { const double G = std::max (grOut, 0.0), Gp = std::pow (G, d3p); kr = d3a * Gp / (1.0 + Gp / std::pow (std::max (0.1, d3cap), d3p)); }
                }
                const double g = gPrev + (gNow - gPrev) * (double) (tick + 1) / factor;
                tick = (tick + 1 == factor) ? 0 : tick + 1;
                // gain-cell ripple: the cell follows the rectified signal a little (twice the signal frequency),
                // low-passed by the cell, with a depth that grows with gain reduction -> odd harmonics like the opto
                double rg = 1.0;
                if (kr > 0)
                {
                    const double ar = std::abs (nch == 2 ? 0.5 * (u[0] + u[1]) : u[0]);
                    ripEnv += cRipEnv * (ar - ripEnv);
                    double rel = ripEnv > 1e-9 ? ar / ripEnv - 1.0 : 0.0;
                    rel = std::max (-1.0, std::min (3.0, rel));
                    if (ripHpOn) rel = ripHp2.process (ripHp1.process (rel));        // keep only the waveform ripple
                    rg = std::exp (-0.11512925464970229 * kr * ripLp.process (rel));
                }
                // MIX blends the Leveler only: the dry part is the (look-ahead delayed) Leveler input
                const double wd = mix * g * rg * gOut + (1.0 - mix);
                for (int c = 0; c < nch; ++c) x[c] = aheadDel[c].process (x[c]) * wd;
                if (grTrace) grTrace[i] = grOut;
            }
            else if (grTrace) grTrace[i] = 0;
            for (int c = 0; c < nch; ++c) x[c] = bl[c].process (2, x[c]);
            // ---- Clipper
            if (set.clipOn)
                for (int c = 0; c < nch; ++c)
                {
                    const double v = x[c] * cg;
                    const double a = std::abs (v) * invC;
                    if (a > clipMax) clipMax = a;
                    double sh;
                    if (a < 0.02) sh = 1.0;
                    else if (a < kClipTabMax) { const double t = a * kClipTabScale; const int k = (int) t; const double f = t - k; sh = clipTab[(size_t) k] * (1 - f) + clipTab[(size_t) k + 1] * f; }
                    else sh = 1.0 / std::pow (1.0 + std::pow (a, cn), 1.0 / cn);
                    const double y = (1.0 - leak) * v * sh + leak * v;
                    x[c] = y * invCg * cOut;
                }
            // ---- slot 3 (+ the final complete filter)
            for (int c = 0; c < nch; ++c) ch[c][i] = bl[c].process (3, x[c]);
        }
    }

    double gainReductionDb() const { return grOut; }
    // meters: max GR (dB) and max clipper drive (1 = at the clip point) since the last call
    double takeGrMax() { const double v = grMax; grMax = grOut; return v; }
    double takeClipMax() { const double v = clipMax; clipMax = 0; return v; }
    bool filtersActive() const { return bl[0].active(); }
    int aheadSamplesBase() const { return aheadBase; }
    // fixed latency of the engine in session-rate samples (Pre HF filter) + AHEAD
    int latencyBase() const { return 1 + aheadBase; }
    // low-frequency group delay of the band-limiting filters in session-rate samples (for latency reporting)
    double filterDelayBase() const { return bl[0].groupDelaySec() * fs; }

private:
    void designFixed()
    {
        const double f0 = model[M::hfF], q0 = model[M::hfQ];
        auto hfTarget = [f0, q0] (double f) { const double x = f / f0; return 1.0 / std::sqrt ((1 - x * x) * (1 - x * x) + (x / q0) * (x / q0)); };
        for (int c = 0; c < 2; ++c)
        {
            lf[c].analog2 (model[M::lfF], model[M::lfQ], model[M::lf1fp], model[M::lf1qp], fo);
            lfSh[c].analog2 (model[M::lf2fz], model[M::lf2qz], model[M::lf2fp], model[M::lf2qp], fo);
            // HF roll-off: linear phase like the Nebula kernels, delay = one session-rate sample
            hfFir[c].design (factor, model[M::preDelayUs] * 1e-6 * fo, fo, std::min (0.45 * fo, 40000.0), hfTarget);
            h2shelf[c].lowShelf1 (model[M::h2ShelfF], model[M::h2ShelfDb], fo);
            h2hp1[c].highpass (model[M::h2HpF], 0.7071, fo); h2hp2[c] = h2hp1[c];
        }
        preTrim = std::pow (10.0, model[M::preTrimDb] / 20.0);
        envRelC = std::exp (-1.0 / (model[M::envRelMs] * 1e-3 * fo));
        sm = 1.0 - std::exp (-1.0 / (0.02 * fo));
        const double cn = model[M::clipN];
        clipTab.resize ((size_t) (kClipTabMax * kClipTabScale) + 2);
        for (size_t k = 0; k < clipTab.size(); ++k) { const double a = (double) k / kClipTabScale; clipTab[k] = 1.0 / std::pow (1.0 + std::pow (a, cn), 1.0 / cn); }
    }

    void buildFilters (int lay)
    {
        for (int c = 0; c < 2; ++c)
        {
            if (lay <= 0) bl[c].off();
            else bl[c].configure (layoutSpecs (lay), fo);
        }
    }

    void updateDetector (bool reset)
    {
        namespace I = M::P;
        // EMPHASIS interpolation
        const double e = std::max (0.0, std::min (1.0, (set.emphasis + 100.0) / 200.0)) * 4.0;
        const int ei = std::min (3, (int) e); const double ef = e - ei;
        auto E = [&] (int k) { const double* p = model.v.data() + M::emph + 3 * ei + k; return p[0] * (1 - ef) + p[3] * ef; };
        const double fc = std::max (1.0, E (0));
        wl = E (1);
        const double tl = E (2);
        split.setHzLP (fc, fs);
        lfAtt.setMs (tl, fs);
        lfRel.setMs (P[I::lfTr] > 0 ? P[I::lfTr] : 1.0, fs);
        cM.setMs (P[I::tauM], fs);
        relP = std::exp (-1.0 / (std::max (0.01, P[I::tauP]) * 1e-3 * fs));
        beta = P[I::beta];
        auto co = [&] (double ms) { return ms <= 0 ? 1.0 : 1.0 - std::exp (-1.0 / (ms * 1e-3 * fs)); };
        cTa1 = co (P[I::ta1]); cTa2 = co (P[I::ta2]); cTa3 = co (P[I::ta3]);
        cTr1 = co (P[I::tr1]); cTr2 = co (P[I::tr2]); cTr3 = co (P[I::tr3]);
        w1 = P[I::w1]; w2 = P[I::w2]; w3 = 1.0 - w1 - w2; Eexp = std::max (1e-3, P[I::E]); pexp = P[I::pexp];
        cap2 = P[I::cap2] > 0 ? P[I::cap2] : 1e9; cap3 = P[I::cap3] > 0 ? P[I::cap3] : 1e9;
        detOff = P[I::detOff];
        qRel = P[I::qRel];
        d3a = P[I::d3a]; d3p = P[I::d3p]; d3cap = P[I::d3cap];
        ripLp.setHzLP (std::max (1.0, P[I::d3fc]), fo); cRipEnv = 1.0 - std::exp (-1.0 / (0.02 * fo));
        ripHpOn = model[M::ripHpF] > 0;
        if (ripHpOn) { ripHp1.highpass (model[M::ripHpF], 0.5412, fo); ripHp2.highpass (model[M::ripHpF], 1.3066, fo); }
        if (reset) { G1 = G2 = G3 = 0; }
    }

    inline void detect (double s) noexcept
    {
        // band split (complementary 1st order)
        const double lo = split.process (s), hi = s - lo;
        // level estimators: mean-square (x2 = sine peak) blended with a peak follower
        msH += cM.c * (2.0 * hi * hi - msH);
        msL += cM.c * (2.0 * lo * lo - msL);
        const double ah = std::abs (hi), al = std::abs (lo);
        pkH = ah > pkH ? ah : pkH * relP;
        pkL = al > pkL ? al : pkL * relP;
        const double lh = (1 - beta) * std::sqrt (msH) + beta * pkH;
        double ll = ((1 - beta) * std::sqrt (msL) + beta * pkL) * wl;
        // slow attack on the bass band (EMPHASIS)
        lfSm += (ll > lfSm ? lfAtt.c : lfRel.c) * (ll - lfSm);
        const double lev = std::sqrt (lh * lh + lfSm * lfSm);
        const double d = 20.0 * std::log10 (lev + 1e-12) + detOff;
        const double T = staticGR (P, d, r);
        // opto cell: three parallel states
        double t1 = w1 * T, t2 = std::min (w2 * T, cap2), t3 = std::min (w3 * T, cap3);
        // release speed grows with the amount of gain reduction (light-dependent resistor):
        // the fast and medium states release with time constant tr / (GR in dB)^qRel
        const double kf = qRel > 0 ? std::pow (std::max (grOut, 0.05), qRel) : 1.0;
        double e1 = t1 - G1;
        if (e1 > 0) { const double k = 1.0 + std::pow (e1 / Eexp, pexp); G1 += e1 * std::min (1.0, cTa1 * k); }
        else G1 += e1 * std::min (1.0, cTr1 * kf);
        const double e2 = t2 - G2; G2 += e2 * (e2 > 0 ? cTa2 : std::min (1.0, cTr2 * kf));
        const double e3 = t3 - G3; G3 += e3 * (e3 > 0 ? cTa3 : cTr3);
        grOut = G1 + G2 + G3;
        if (grOut > grMax) grMax = grOut;
    }

    Model model;
    Settings set;
    double fs = 48000, fo = 48000; int factor = 1; bool filtersAuto = false, prepared = false;
    int lastLayout = -2, maxAhead = 600, aheadBase = 0;
    // Pre
    Biquad lf[2], lfSh[2], h2shelf[2], h2hp1[2], h2hp2[2]; SymFir hfFir[2]; double env[2] {}, envRelC = 0.999, preTrim = 1;
    OnePole ripLp; Biquad ripHp1, ripHp2; bool ripHpOn = false; double d3a = 0, d3p = 2, d3cap = 0, kr = 0, ripEnv = 0, cRipEnv = 0.001;
    double level = 1, k2 = 0, k3 = 0, tLevel = 1, tK2 = 0, sm = 0.001;
    double tGOut = 1, tCg = 1, tCOut = 1, tMix = 1, grMax = 0, clipMax = 0;
    double g2Cache[2] { 1, 1 }; unsigned g2Count[2] { 0, 0 };
    static constexpr double kClipTabMax = 16.0, kClipTabScale = 512.0;
    std::vector<double> clipTab;
    // filters
    fbl::BandLimit<4> bl[2];
    // Comp
    const double* P = nullptr;
    double gOut = 1, r = 0;
    OnePole split, cM, lfAtt, lfRel; double relP = 0.99, beta = 0, wl = 1, lfSm = 0;
    double msH = 0, msL = 0, pkH = 0, pkL = 0;
    double cTa1 = 1, cTa2 = 1, cTa3 = 1, cTr1 = 1, cTr2 = 1, cTr3 = 1, w1 = 1, w2 = 0, w3 = 0, Eexp = 3, pexp = 1;
    double cap2 = 1e9, cap3 = 1e9, detOff = 0, qRel = 0;
    double G1 = 0, G2 = 0, G3 = 0, grOut = 0, gPrev = 1, gNow = 1; int tick = 0;
    Delay aheadDel[2], preByp[2];
    // Clipper
    double cg = 1, cOut = 1;
    double mix = 1;
};
} // namespace la2
