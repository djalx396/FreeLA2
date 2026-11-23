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
if len(sys.argv) > 3 and sys.argv[3] == 'fresh': corr = np.zeros((3, 67))
best = [(1e9, corr[j].copy()) for j in range(3)]       # v0.2: damped, bounded, keep the best pass per curve
for it in range(its + 1):
    T = tables(PROG, corr); q = prog_dict(params, T, EM); q['d3'] = json.load(open(prog_file(PROG)))['d3']; m = build_model.build({prog_file(PROG): q})
    tot = []
    for j, t in enumerate(['GR03', 'GR08', 'GR15']):
        s = sim_st(m, t)[:67, 1]; a = M['%s_%s_st' % (PROG, t)][:67, 1]
        d = (a - s) * (a > 0.5); e = np.sqrt(np.mean(d[a > 0.5] ** 2)); tot.append(e)
        if e < best[j][0]: best[j] = (e, corr[j].copy())
        corr[j] = np.clip(corr[j] + 0.5 * d, -2.0, 2.0)
    print('iter %d: rms %s' % (it, ' '.join('%.3f' % v for v in tot)), flush=True)
corr = np.array([b[1] for b in best]); print('best', ' '.join('%.3f' % b[0] for b in best))
np.save(cf, corr)
T = tables(PROG, corr); q = prog_dict(params, T, EM); q['d3'] = json.load(open(prog_file(PROG)))['d3']; json.dump(q, open(prog_file(PROG), 'w'))
