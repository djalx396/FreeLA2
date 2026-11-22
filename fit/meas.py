# measurement helpers on arrays (signal timeline at 96 kHz)
import sys, numpy as np
sys.path.insert(0,'/home/claude/la2a_ana')
from ana import load, sig, seg, tone, db, SR
def steps_arr(y, section, segname, f0, levels, K=5, t0=0.12, ch=0):
    a, n = seg(section, segname); out = []
    for k, L in enumerate(levels):
        b = a + k * 24000 + int(t0 * SR); nc = int(0.12 * f0); Ls = int(round(nc * SR / f0))
        c = tone(y[b:b + Ls, ch], f0, K); out.append((L, db(c[0]) - L, [db(c[i]) - db(c[0]) for i in range(1, K)]))
    return out
UP = list(range(-60, 7)); DN = list(range(5, -61, -1)); LV = list(range(-40, 7, 2))
