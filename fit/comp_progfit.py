"""v0.2: refit the Leveler timing on the sine bursts (DYN/DYNS) AND the program material (PROG: drums, bass, vocal).
Each pass is simulated in render order (previous section first), so the carry-over matches the Nebula renders.
usage: python3 comp_progfit.py comp|comp_LIMIT [max_nfev]
"""
import sys, json, time, numpy as np
sys.path.insert(0, '/home/claude/freela2/fit'); sys.path.insert(0, '/home/claude/la2a_ana')
from multiprocessing import Pool
from scipy.signal import resample_poly
from scipy.optimize import least_squares
import la2, build_model
from comp_fit import *
FS = 24000; DEC = 4
PROG = sys.argv[1] if len(sys.argv) > 1 else 'comp'
EM = json.load(open('emph.json'))
TG = targets(PROG)
BOUNDS = dict(tauM=(0.3, 60), tauP=(0.3, 300), beta=(0, 1), w1=(0.1, 0.95), w2=(0.0, 0.7), ta1=(0.5, 150), E=(0.3, 40), pexp=(0, 3),
              ta2=(3, 2000), ta3=(30, 8000), tr1=(3, 400), tr2=(30, 5000), tr3=(200, 30000), cap2=(0.3, 40), cap3=(0.3, 40), qRel=(0, 3))
_X = {}
def xlow(sec):
    if sec not in _X: _X[sec] = resample_poly(sig(sec), 1, DEC, axis=0)
    return _X[sec]
# passes: (knob name, section before PROG). LIMIT has PROG only at GR08.
PASSES = [('GR08', 'DYN'), ('GR15', 'DYN'), ('GR03', 'DYNS')] if PROG == 'comp' else [('GR08', 'DYN'), ('GR15', 'DYNS'), ('GR03', 'DYNS')]
HAS_PROG = {'GR08', 'GR15', 'GR03'} if PROG == 'comp' else {'GR08'}
BLK = FS // 1000
def prog_gr(y, x):
    """GR trace (dB) in 1 ms blocks from 2.0 s to 26.7 s, and a mask of blocks with signal"""
    a, b = int(2.0 * FS), int(26.7 * FS); m = (b - a) // BLK
    ry = np.sqrt(np.mean(y[a:a + m * BLK, 0].reshape(m, BLK) ** 2, 1)); rx = np.sqrt(np.mean(x[a:a + m * BLK, 0].reshape(m, BLK) ** 2, 1))
    return REF[PROG] - 20 * np.log10(ry / (rx + 1e-20) + 1e-20), 20 * np.log10(rx + 1e-20) > -45
PT = {}
for t in HAS_PROG:
    ref = resample_poly(load('%s_%s__PROG' % (PROG, t)), 1, DEC, axis=0)
    g, mk = prog_gr(ref, xlow('PROG')); PT[t] = (g, mk)
import os
WP = float(os.environ.get('WP', '0.5'))          # weight of the program material relative to the bursts (v0.4 refit: 1.0)
def model_for(p):
    T = tables(PROG)
    pp = json.load(open(prog_file(PROG)))
    q = prog_dict(dict(zip(PKEYS, p)), T, EM); q['d3'] = pp['d3']
    return build_model.build({prog_file(PROG): q})
def resid(p):
    pen = 50 * (p[3] + p[4] - 0.98) if p[3] + p[4] > 0.98 else 0
    m = model_for(p); S = {}; r = []
    for t, prev in PASSES:
        k = dict(zip(['GR03', 'GR08', 'GR15'], KNOB[PROG]))[t]
        secs = [prev] + (['PROG'] if t in HAS_PROG else [])
        outs = la2.run_seq(m, [xlow(s) for s in secs], fs=FS, preOn=0, clipOn=0, limit=int(PROG != 'comp'), reduction=100 * k, filterMode=0, filtersForce=0)
        for s, (y, g) in zip(secs, outs): S[(t, s)] = y
    for t, sec, s, a, n, g in TG:
        if (t, sec) not in S: continue
        gm = env_gr(S[(t, sec)], xlow(sec), a // DEC, n // DEC, REF[PROG], blk=FS // 1000)
        L = min(len(gm), len(g)); e = gm[:L] - g[:L]
        w = np.ones(L); w[:900] = 0
        tt = np.arange(L) / 1000.0; w[(tt >= 1.0) & (tt < 1.12)] *= 3
        r.append(e[::2] * w[::2])
    for t in HAS_PROG:
        g, mk = prog_gr(S[(t, 'PROG')], xlow('PROG')); gr, mkr = PT[t]
        e = np.clip(g - gr, -6, 6)[mk & mkr]
        r.append(WP * e[::2])
    return np.concatenate(r + [[pen]])
def _col(args):
    p, i, h = args; q = np.array(p); q[i] += h; return resid(q)
if __name__ == '__main__':
    p0 = json.load(open(prog_file(PROG)))['params']
    x0 = np.array([p0[k] for k in PKEYS])
    lo = np.array([BOUNDS[k][0] for k in PKEYS]); hi = np.array([BOUNDS[k][1] for k in PKEYS]); x0 = np.clip(x0, lo, hi)
    pool = Pool(2)
    def jac(p):
        f0 = resid(p); hs = [max(1e-3, abs(v) * 0.02) for v in p]
        R = pool.map(_col, [(p, i, hs[i]) for i in range(len(p))])
        return np.stack([(R[i] - f0) / hs[i] for i in range(len(p))], 1)
    t0 = time.time()
    r0 = resid(x0); print('start rms %.3f dB (%.1f s per eval, %d residuals)' % (np.sqrt(np.mean(r0 ** 2)), time.time() - t0, len(r0)), flush=True)
    res = least_squares(resid, x0, jac=jac, bounds=(lo, hi), x_scale='jac', max_nfev=int(sys.argv[2]) if len(sys.argv) > 2 else 20, verbose=1)
    print('end rms %.3f dB in %.0f s' % (np.sqrt(np.mean(res.fun ** 2)), time.time() - t0))
    best = dict(zip(PKEYS, res.x)); print(json.dumps({k: round(v, 4) for k, v in best.items()}))
    pp = json.load(open(prog_file(PROG))); q = prog_dict(best, tables(PROG), EM); q['d3'] = pp['d3']
    json.dump(q, open(prog_file(PROG), 'w'))
