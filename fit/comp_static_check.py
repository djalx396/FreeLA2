import sys, json, numpy as np
sys.path.insert(0, '/home/claude/freela2/fit')
import la2, build_model
from comp_fit import *
from meas import steps_arr, UP, DN
PROG = sys.argv[1]
def sim_st(m, t):
    k = dict(zip(['GR03', 'GR08', 'GR15'], KNOB[PROG]))[t]
    y = la2.run(m, sig('ST1K'), preOn=0, clipOn=0, limit=int(PROG != 'comp'), reduction=100 * k, filterMode=0, filtersForce=0)
    return np.array([[L, REF[PROG] - g] for L, g, h in steps_arr(y, 'ST1K', 'steps_1k_updown', 1000, UP + DN, t0=0.15)])
if __name__ == '__main__':
    m = build_model.build(); M = np.load('meas_comp.npz')
    out = {}
    for t in ['GR03', 'GR08', 'GR15']:
        s = sim_st(m, t); a = M['%s_%s_st' % (PROG, t)]
        out[t] = (a, s)
        up = slice(0, 67)
        d = s[up, 1] - a[up, 1]; L = a[up, 0]; msk = a[up, 1] > 0.5
        print('%s up: rms %.3f dB (GR>0.5) | at -20/-10/0/+6: meas %s model %s' % (t, np.sqrt(np.mean(d[msk] ** 2)),
              ' '.join('%.2f' % a[int(x + 60), 1] for x in (-20, -10, 0, 6)), ' '.join('%.2f' % s[int(x + 60), 1] for x in (-20, -10, 0, 6))))
        dn = slice(67, 133); Ld = a[dn, 0]
        print('      down (hysteresis) at -14/-20/-26/-30/-40: meas %s model %s' % (' '.join('%.2f' % a[67 + int(5 - x), 1] for x in (-14, -20, -26, -30, -40)),
              ' '.join('%.2f' % s[67 + int(5 - x), 1] for x in (-14, -20, -26, -30, -40))))
    np.save('st_sim_%s.npy' % PROG, np.array([out[t][1] for t in ['GR03', 'GR08', 'GR15']]))
