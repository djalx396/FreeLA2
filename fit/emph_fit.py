import sys, json, numpy as np
sys.path.insert(0, '/home/claude/freela2/fit'); sys.path.insert(0, '/home/claude/la2a_ana')
from scipy.signal import resample_poly
from scipy.optimize import least_squares
import la2, build_model
from comp_fit import *
FS = 24000; DEC = 4
FREQS = [30, 60, 100, 200, 500, 1000, 2000, 5000, 10000]
BF = [50, 100, 300, 1000]
def tone_gr(y, x, a, f, fs, ref):
    t0, t1 = 0.7, 1.15; nc = int((t1 - t0) * f); L = int(round(nc * fs / f)); b = a + int(t0 * fs)
    tt = np.arange(L) / fs; e = np.exp(-2j * np.pi * f * tt)
    return ref - 20 * np.log10(abs(np.sum(y[b:b + L, 0] * e)) / abs(np.sum(x[b:b + L, 0] * e)))
def burst_gr(y, x, a, n, f, fs, ref, times):
    P = int(round(fs / f)); seg = lambda v: v[a:a + n, 0]
    t = np.arange(n) / fs; z = np.exp(-2j * np.pi * f * t)
    def env(v):
        c = np.cumsum(np.concatenate([[0], seg(v) * z])); return np.abs(c[P:] - c[:-P])
    ey, ex = env(y), env(x); tc = (np.arange(len(ey)) + P / 2) / fs
    g = ref - 20 * np.log10(ey / (ex + 1e-20) + 1e-20)
    return np.interp(times, tc, g)
TIMES = np.array([1.01, 1.02, 1.05, 1.1, 1.3, 1.99, 2.1, 3.0])
def measure(y, x, fs, ref):
    st = [tone_gr(y, x, seg('DETF', 'det_%d_-10' % f)[0] * fs // SR, f, fs, ref) for f in FREQS]
    dy = [burst_gr(y, x, seg('DYNF', 'burst_%d_-30_up20_hold1' % f)[0] * fs // SR, seg('DYNF', 'burst_%d_-30_up20_hold1' % f)[1] * fs // SR, f, fs, ref, TIMES) for f in BF]
    return np.array(st), np.array(dy)
_X = {}
def xl(sec):
    if sec not in _X: _X[sec] = resample_poly(sig(sec), 1, DEC, axis=0)
    return _X[sec]
def meas_target(prog, ek):
    y = load('%s_EMPHASIS_%s__DETF' % (prog, ek)); x = sig('DETF')
    st = [tone_gr(y, x, seg('DETF', 'det_%d_-10' % f)[0], f, SR, REF[prog]) for f in FREQS]
    y2 = load('%s_EMPHASIS_%s__DYNF' % (prog, ek)); x2 = sig('DYNF')
    dy = [burst_gr(y2, x2, *seg('DYNF', 'burst_%d_-30_up20_hold1' % f), f, SR, REF[prog], TIMES) for f in BF]
    return np.array(st), np.array(dy)
def sim(em, prog, e):
    m = build_model.build({'emph.json': em, prog_file(prog): prog_dict(json.load(open(prog_file(prog)))['params'], tables(prog), em)})
    k = KNOB[prog][1]
    out = []
    for sec in ['DETF', 'DYNF']:
        x = xl(sec); y = la2.run(m, x, fs=FS, preOn=0, clipOn=0, limit=int(prog != 'comp'), reduction=100 * k, emphasis=e, filterMode=0, filtersForce=0)
        out.append((y, x))
    (y1, x1), (y2, x2) = out
    st = [tone_gr(y1, x1, seg('DETF', 'det_%d_-10' % f)[0] // DEC, f, FS, REF[prog]) for f in FREQS]
    dy = [burst_gr(y2, x2, seg('DYNF', 'burst_%d_-30_up20_hold1' % f)[0] // DEC, seg('DYNF', 'burst_%d_-30_up20_hold1' % f)[1] // DEC, f, FS, REF[prog], TIMES) for f in BF]
    return np.array(st), np.array(dy)
if __name__ == '__main__':
    em = json.load(open('emph.json'))
    prog = 'comp'
    for k, ek, e in [(1, '025', -50), (2, '050', 0), (3, '075', 50), (4, '100', 100)]:
        mst, mdy = meas_target(prog, ek)
        def res(p):
            q = json.loads(json.dumps(em)); q['pts'][k] = [float(p[0]), float(p[1]), float(p[2])]
            sst, sdy = sim(q, prog, e)
            return np.concatenate([sst - mst, 0.5 * (sdy - mdy).ravel()])
        p0 = em['pts'][k]
        r = least_squares(res, p0, bounds=([5, 0.0, 0.0], [2000, 1.0, 400]), x_scale=[50, 0.1, 30], diff_step=0.05, max_nfev=30)
        em['pts'][k] = [float(v) for v in r.x]
        sst, sdy = sim(em, prog, e)
        print('EMPH %s: fc %.1f Hz wl %.3f tl %.1f ms | static rms %.2f | meas %s | model %s' % (ek, *r.x, np.sqrt(np.mean((sst - mst) ** 2)),
              ' '.join('%.1f' % v for v in mst), ' '.join('%.1f' % v for v in sst)), flush=True)
        json.dump(em, open('emph.json', 'w'))
