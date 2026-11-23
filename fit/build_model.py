# Assemble the flat model array from the fitted parts (JSON files in fit/), write docs/freela2_model_v0.json
import json, numpy as np, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import la2
HERE = os.path.dirname(os.path.abspath(__file__))
def J0(name, default=None):
    p = os.path.join(HERE, name)
    return json.load(open(p)) if os.path.exists(p) else default
def build(over=None):
    over = over or {}
    def J(name, default=None):
        return over[name] if name in over else J0(name, default)
    m = np.zeros(la2.MSIZE)
    eq = J('pre_eq.json')
    fz1, qz1, fp1, qp1, fz2, qz2, fp2, qp2 = eq['lfSec']
    m[la2.lfF], m[la2.lfQ], m[la2.LF1FP], m[la2.LF1QP] = fz1, qz1, fp1, qp1
    m[la2.LF2FZ], m[la2.LF2QZ], m[la2.LF2FP], m[la2.LF2QP] = fz2, qz2, fp2, qp2
    m[la2.PRETRIM] = eq.get('trimDb', 0.027); m[44] = eq.get('delayUs', 1.884)
    m[la2.hfF], m[la2.hfQ] = eq['hfF'], eq['hfQ']
    for k, r in enumerate(['44100', '48000', '88200', '96000']):
        m[la2.HFTAB + 5 * k: la2.HFTAB + 5 * k + 5] = eq['hfTab'][r]
    ph = J('pre_h.json')
    m[la2.k2], m[la2.k3], m[la2.h2ShelfF], m[la2.h2ShelfDb], m[la2.h2HpF], m[la2.envRelMs] = \
        ph['k2'], ph['k3'], ph['shF'], ph['shDb'], ph['hpF'], ph['envRelMs']
    m[la2.g2Tab:la2.g2Tab + 24] = ph['g2']
    cl = J('clip.json', dict(c=1.5, n=4.0, leak=0.0))
    m[la2.clipC], m[la2.clipN], m[42] = cl['c'], cl['n'], cl.get('leak', 0.0)
    m[45] = J('ripple.json', dict(hpF=0.0))['hpF']
    em = J('emph.json', dict(pts=[[1, 1, 0], [60, 0.5, 30], [120, 0.3, 60], [200, 0.2, 80], [300, 0.15, 100]]))
    for k, (fc, wl, tl) in enumerate(em['pts']):
        m[la2.EMPH + 3 * k: la2.EMPH + 3 * k + 3] = [fc, wl, tl]
    for lim, name in [(0, 'comp_COMP.json'), (1, 'comp_LIMIT.json')]:
        c = J(name)
        if c is None: continue
        base = la2.PROG0 + lim * la2.PSIZE
        for k, v in c['params'].items(): m[base + la2.P[k]] = v
        m[base + la2.P['u0']], m[base + la2.P['du']], m[base + la2.P['nu']] = c['u0'], c['du'], len(c['curves'][0])
        for k in range(3): m[base + la2.P['curves'] + k * la2.NUMAX: base + la2.P['curves'] + k * la2.NUMAX + len(c['curves'][k])] = c['curves'][k]
        m[base + la2.THETA: base + la2.THETA + 101] = c['theta']
        m[base + la2.CPOS: base + la2.CPOS + 101] = c['cpos']
        d3 = c.get('d3', dict(a=0, p=2, cap=0, fc=300)); m[base + la2.P['d3a']], m[base + la2.P['d3p']], m[base + la2.D3CAP], m[base + la2.D3FC] = d3['a'], d3['p'], d3['cap'], d3['fc']
    return m
if __name__ == '__main__':
    m = build()
    os.makedirs(os.path.join(HERE, '..', 'docs'), exist_ok=True)
    json.dump(dict(version='v0', size=len(m), values=list(m)), open(os.path.join(HERE, '..', 'docs', 'freela2_model_v0.json'), 'w'))
    print('model', len(m))
