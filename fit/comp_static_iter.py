import sys, os, json, numpy as np
sys.path.insert(0, '/home/claude/freela2/fit')
import build_model
from comp_fit import *
from comp_static_check import sim_st
PROG = sys.argv[1]; its = int(sys.argv[2]) if len(sys.argv) > 2 else 2
EM = json.load(open('emph.json'))
M = np.load('meas_comp.npz')
cf = 'corr_%s.npy' % PROG
corr = np.load(cf) if os.path.exists(cf) else np.zeros((3, 67))
params = json.load(open(prog_file(PROG)))['params']
for it in range(its):
    T = tables(PROG, corr); m = build_model.build({prog_file(PROG): prog_dict(params, T, EM)})
    tot = []
    for j, t in enumerate(['GR03', 'GR08', 'GR15']):
        s = sim_st(m, t)[:67, 1]; a = M['%s_%s_st' % (PROG, t)][:67, 1]
        d = (a - s) * (a > 0.5); corr[j] += 0.8 * d; tot.append(np.sqrt(np.mean(d[a > 0.5] ** 2)))
    print('iter %d: rms before correction %s' % (it, ' '.join('%.3f' % v for v in tot)), flush=True)
np.save(cf, corr)
T = tables(PROG, corr); json.dump(prog_dict(params, T, EM), open(prog_file(PROG), 'w'))
