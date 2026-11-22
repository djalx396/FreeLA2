import sys, json, numpy as np
sys.path.insert(0, '/home/claude/freela2/fit')
from comp_timefit import *
p = json.load(open(prog_file(PROG)))['params']; x = np.array([p[k] for k in PKEYS])
S = sim_env(x)
TA = [0.001, 0.005, 0.01, 0.02, 0.05, 0.1, 0.5, 0.98]; TR = [0.01, 0.05, 0.1, 0.3, 1, 2, 4]
for t, sec, s, a, n, g in TG:
    y, xx = S[(t, sec)]; gm = env_gr(y, xx, a // DEC, n // DEC, REF[PROG], blk=FS // 1000)
    hold = float(s.split('hold')[1]) if 'hold' in s else 0.1
    def at(v, tt): i = int(round(tt * 1000)); return v[i] if i < len(v) else np.nan
    if 'train' in s: continue
    print('%s %-28s ATT ' % (t, s[11:]) + ' '.join('%5.2f/%5.2f' % (at(g, 1 + d), at(gm, 1 + d)) for d in TA[:6]))
    print('%s %-28s REL ' % (t, '') + ' '.join('%5.2f/%5.2f' % (at(g, 1 + hold + d), at(gm, 1 + hold + d)) for d in TR))
