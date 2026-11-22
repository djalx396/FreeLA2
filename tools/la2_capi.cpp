// C API around the engine, for the Python fitting tools (not part of the plugin).
#include "../Source/LA2Engine.h"
extern "C"
{
    void* la2_new (const double* model, int n) { auto* e = new la2::Engine(); e->setModel (la2::Model (model, n)); return e; }
    void  la2_free (void* p) { delete (la2::Engine*) p; }
    // settings: preOn compOn clipOn pushed drive preOutDb limit inputDb reduction emphasis gainDb aheadMs clipIn clipOutDb mix filterMode filtersForce
    static la2::Settings toSettings (const double* s)
    {
        la2::Settings t;
        t.preOn = s[0] > 0.5; t.compOn = s[1] > 0.5; t.clipOn = s[2] > 0.5; t.pushed = s[3] > 0.5; t.drive = s[4];
        t.preOutDb = s[5]; t.limit = s[6] > 0.5; t.inputDb = s[7]; t.reduction = s[8]; t.emphasis = s[9]; t.gainDb = s[10];
        t.aheadMs = s[11]; t.clipIn = s[12]; t.clipOutDb = s[13]; t.mix = s[14]; t.filterMode = (int) s[15]; t.filtersForce = (int) s[16];
        return t;
    }
    void la2_prepare (void* p, double fs, int factor, const double* s) { auto* e = (la2::Engine*) p; e->update (toSettings (s), true); e->prepare (fs, factor); }
    void la2_update (void* p, const double* s) { ((la2::Engine*) p)->update (toSettings (s)); }
    void la2_reset (void* p) { ((la2::Engine*) p)->reset(); }
    void la2_process (void* p, double* L, double* R, int n, double* gr) { ((la2::Engine*) p)->process (L, R, n, gr); }
    double la2_filter_delay (void* p) { return ((la2::Engine*) p)->filterDelayBase(); }
}
