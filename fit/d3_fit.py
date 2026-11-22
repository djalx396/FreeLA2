import sys, json, numpy as np
sys.path.insert(0, '/home/claude/freela2/fit')
from scipy.optimize import least_squares
import la2, build_model
from comp_fit import *
from meas import steps_arr, UP, DN
PROG = sys.argv[1]; lim = int(PROG != 'comp')
LV1 = [-16, -12, -10, -8, -6, -4, -2, 0, 2, 4, 6]
def meas_h3():
    out = {}
    for t, sec, sg, f0, lev in [('GR08', 'ST1K', 'steps_1k_updown', 1000, UP + DN), ('GR08', 'ST100', 'steps_100_up', 100, UP), ('GR08', 'ST5K', 'steps_5k_up', 5000, list(range(-60, 7, 2))),
                                ('GR15', 'ST1K', 'steps_1k_updown', 1000, UP + DN), ('GR03', 'ST1K', 'steps_1k_updown', 1000, UP + DN)]:
        try: y = load('%s_%s__%s' % (PROG, t, sec))
        except Exception: continue
        d = dict((r[0], r[2][1]) for r in steps_arr(y, sec, sg, f0, lev, t0=0.15)[:len([l for l in lev if True])])
        out[(t, sec)] = (sg, f0, lev, d)
    return out
MH = meas_h3()
pp = json.load(open(prog_file(PROG)))
def model_with(d3):
    q = dict(pp); q['d3'] = d3
    return build_model.build({prog_file(PROG): q})
def sim_h3(m):
    out = {}
    for (t, sec), (sg, f0, lev, d) in MH.items():
        k = dict(zip(['GR03', 'GR08', 'GR15'], KNOB[PROG]))[t]
        y = la2.run(m, sig(sec), preOn=0, clipOn=0, limit=lim, reduction=100 * k, filterMode=0)
        out[(t, sec)] = dict((r[0], r[2][1]) for r in steps_arr(y, sec, sg, f0, lev, t0=0.15))
    return out
def res(p):
    S = sim_h3(model_with(dict(a=10 ** p[0], p=p[1], cap=10 ** p[2], fc=10 ** p[3]))); r = []
    for key, (sg, f0, lev, d) in MH.items():
        for L in LV1:
            if L in d and d[L] > -90: r.append(np.clip(S[key][L], -120, 0) - d[L])
    return np.array(r)
if __name__ == '__main__':
    p0 = [np.log10(0.006), 2.5, np.log10(12.0), np.log10(250)]
    r = least_squares(res, p0, bounds=([-6, 0.5, -0.5, 1.0], [1, 6, 1.8, 3.5]), diff_step=0.02, max_nfev=40)
    d3 = dict(a=10 ** r.x[0], p=r.x[1], cap=10 ** r.x[2], fc=10 ** r.x[3])
    print(PROG, 'd3', {k: round(v, 5) for k, v in d3.items()}, 'rms %.2f dB' % np.sqrt(np.mean(r.fun ** 2)))
    S = sim_h3(model_with(d3))
    for key, (sg, f0, lev, d) in MH.items():
        print('  %s %s H3 dBc at %s: nebula %s | model %s' % (key[0], key[1], LV1[::2], ' '.join('%.1f' % d[L] for L in LV1[::2] if L in d), ' '.join('%.1f' % S[key][L] for L in LV1[::2] if L in d)))
    pp['d3'] = d3; json.dump(pp, open(prog_file(PROG), 'w'))
