import sys, json, time, numpy as np
sys.path.insert(0, '/home/claude/freela2/fit'); sys.path.insert(0, '/home/claude/la2a_ana')
from multiprocessing import Pool
from scipy.signal import resample_poly
from scipy.optimize import least_squares
import la2, build_model
from comp_fit import *
FS = 24000; DEC = 4
_X = {}
def xlow(sec):
    if sec not in _X: _X[sec] = resample_poly(sig(sec), 1, DEC, axis=0)
    return _X[sec]
PROG = sys.argv[1] if len(sys.argv) > 1 else 'comp'
EM = json.load(open('emph.json'))
TG = targets(PROG)
BOUNDS = dict(tauM=(0.3, 60), tauP=(0.3, 300), beta=(0, 1), w1=(0.1, 0.95), w2=(0.0, 0.7), ta1=(0.5, 150), E=(0.3, 40), pexp=(0, 3),
              ta2=(3, 2000), ta3=(30, 8000), tr1=(3, 400), tr2=(30, 5000), tr3=(200, 30000), cap2=(0.3, 40), cap3=(0.3, 40), qRel=(0, 3))
CORR = None
_T = {}
def model_for(p):
    if 'T' not in _T: _T['T'] = tables(PROG, CORR)
    return build_model.build({prog_file(PROG): prog_dict(dict(zip(PKEYS, p)), _T['T'], EM), 'emph.json': EM})
def sim_env(p):
    m = model_for(p)
    out = {}
    for t, sec in sorted(set((a[0], a[1]) for a in TG)):
        k = dict(zip(['GR03', 'GR08', 'GR15'], KNOB[PROG]))[t]
        x = xlow(sec)
        y = la2.run(m, x, fs=FS, preOn=0, clipOn=0, limit=int(PROG != 'comp'), reduction=100 * k, filterMode=0, filtersForce=0)
        out[(t, sec)] = (y, x)
    return out
def resid(p):
    if p[3] + p[4] > 0.98: pen = 50 * (p[3] + p[4] - 0.98)
    else: pen = 0
    S = sim_env(p); r = []
    for t, sec, s, a, n, g in TG:
        y, x = S[(t, sec)]
        gm = env_gr(y, x, a // DEC, n // DEC, REF[PROG], blk=FS // 1000)
        L = min(len(gm), len(g)); e = gm[:L] - g[:L]
        w = np.ones(L); w[:900] = 0                       # ignore the first 0.9 s (carry-over from the previous render)
        tt = np.arange(L) / 1000.0
        for edge in (1.0,):
            w[(tt >= edge) & (tt < edge + 0.12)] *= 3       # attack
        r.append(e[::2] * w[::2])
    return np.concatenate(r + [[pen]])
def _col(args):
    p, i, h = args; q = np.array(p); q[i] += h; return resid(q)
if __name__ == '__main__':
    p0 = json.load(open('comp_%s.json' % ('COMP' if PROG == 'comp' else 'LIMIT')))['params']
    DEF = dict(qRel=1.5, tr1=400.0, tr2=3000.0, cap2=15.0, cap3=15.0)
    x0 = np.array([p0[k] if p0.get(k, 0) > 0 else DEF.get(k, 15.0) for k in PKEYS])
    if p0.get('qRel', 0) == 0: x0[PKEYS.index('tr1')] = 400.0; x0[PKEYS.index('tr2')] = 3000.0
    lo = np.array([BOUNDS[k][0] for k in PKEYS]); hi = np.array([BOUNDS[k][1] for k in PKEYS]); x0 = np.clip(x0, lo, hi)
    pool = Pool(2)
    def jac(p):
        f0 = resid(p); cols = []
        hs = [max(1e-3, abs(v) * 0.02) for v in p]
        R = pool.map(_col, [(p, i, hs[i]) for i in range(len(p))])
        return np.stack([(R[i] - f0) / hs[i] for i in range(len(p))], 1)
    t0 = time.time()
    r0 = resid(x0); print('start rms %.3f dB (%.1f s per eval)' % (np.sqrt(np.mean(r0 ** 2)), time.time() - t0), flush=True)
    res = least_squares(resid, x0, jac=jac, bounds=(lo, hi), x_scale='jac', max_nfev=int(sys.argv[2]) if len(sys.argv) > 2 else 25, verbose=1)
    print('end rms %.3f dB in %.0f s' % (np.sqrt(np.mean(res.fun ** 2)), time.time() - t0))
    best = dict(zip(PKEYS, res.x)); print(json.dumps({k: round(v, 4) for k, v in best.items()}))
    T = tables(PROG, CORR); write_prog(PROG, best, T, EM)
