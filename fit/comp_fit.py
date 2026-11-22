import sys, json, time, numpy as np
sys.path.insert(0, '/home/claude/freela2/fit'); sys.path.insert(0, '/home/claude/la2a_ana')
import la2, build_model
from comp_tables import *
from ana import load, sig, seg, SR
REF = {'comp': -0.026, 'comp_LIMIT': -0.006}
PKEYS = ['tauM', 'tauP', 'beta', 'w1', 'w2', 'ta1', 'E', 'pexp', 'ta2', 'ta3', 'tr1', 'tr2', 'tr3', 'cap2', 'cap3', 'qRel']
def env_gr(y, x, a, n, ref, blk=96):
    m = n // blk
    ry = np.sqrt(np.mean(y[a:a + m * blk, 0].reshape(m, blk) ** 2, 1)); rx = np.sqrt(np.mean(x[a:a + m * blk, 0].reshape(m, blk) ** 2, 1))
    return ref - 20 * np.log10(ry / (rx + 1e-20) + 1e-20)
def emph_detoff(em, e=0.5):
    pts = np.array(em['pts']); k = e * 4; i = min(3, int(k)); f = k - i
    fc, wl = pts[i, 0] * (1 - f) + pts[i + 1, 0] * f, pts[i, 1] * (1 - f) + pts[i + 1, 1] * f
    x = 1000.0 / fc; lp = 1 / np.sqrt(1 + x * x); hp = x / np.sqrt(1 + x * x)
    return -20 * np.log10(np.sqrt(hp ** 2 + (wl * lp) ** 2))
def tables(prog, corr=None):
    import os
    cf = 'corr_%s.npy' % prog
    if corr is None and os.path.exists(cf): corr = np.load(cf)
    M = np.load('meas_comp.npz'); pts = []
    for j, t in enumerate(['GR03', 'GR08', 'GR15']):
        a = M['%s_%s_st' % (prog, t)][:67]; L, g = a[:, 0], a[:, 1]
        res = np.median(g[L <= -45]); g = np.where(g < 1.0, np.maximum(0, g - res), g)
        if corr is not None: g = g + corr[j]
        pts.append((L, g))
    return build_tables(pts, KNOB[prog], M['%s_scan' % prog])
def prog_dict(params, T, em):
    p = dict(params); p['detOff'] = emph_detoff(em); p.setdefault('lfTr', 60.0)
    return dict(params=p, u0=T['u0'], du=T['du'], curves=T['curves'], theta=T['theta'], cpos=T['cpos'])
def prog_file(prog): return 'comp_%s.json' % ('COMP' if prog == 'comp' else 'LIMIT')
def write_prog(prog, params, T, em):
    json.dump(prog_dict(params, T, em), open(prog_file(prog), 'w'))
DYN_SEGS = ['burst_1000_-30_up10_hold1', 'burst_1000_-30_up20_hold1', 'burst_1000_-30_up30_hold1', 'burst_1000_-20_up20_hold1',
            'burst_1000_-30_up20_hold0.02', 'burst_1000_-30_up20_hold0.1', 'burst_1000_-30_up20_hold0.5',
            'burst_1000_-30_up20_hold2', 'burst_1000_-30_up20_hold8', 'train_1000_-30_up20']
def targets(prog):
    tg = []
    for t, sec, segs in [('GR08', 'DYN', DYN_SEGS), ('GR15', 'DYN', DYN_SEGS), ('GR03', 'DYNS', ['burst_1000_-30_up20_hold1', 'burst_1000_-30_up20_hold0.1', 'burst_1000_-30_up20_hold2'])]:
        try: y = load('%s_%s__%s' % (prog, t, sec))
        except Exception: continue
        x = sig(sec)
        for s in segs:
            a, n = seg(sec, s); tg.append((t, sec, s, a, n, env_gr(y, x, a, n, REF[prog])))
    return tg
def simulate(model, prog, t, sec):
    k = dict(zip(['GR03', 'GR08', 'GR15'], KNOB[prog]))[t]
    x = sig(sec)
    return la2.run(model, x, preOn=0, clipOn=0, limit=int(prog != 'comp'), reduction=100 * k, filterMode=0, filtersForce=0), x
