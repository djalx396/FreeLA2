"""Compare the FreeLA2 plug-in (tests/compare_nebula.cpp = the real processor) with every Nebula LA-2A render.

All renders run in capture order through ONE plug-in instance (state carries over like the REAPER renders),
each with the Nebula knob settings mapped to FreeLA2's knobs and the same stages enabled.

usage: python3 compare_nebula.py <tag> [oslive filter]      e.g.  A 0 0   |   B 1 1   |   C 2 2
writes /tmp/cmp/<tag>/metrics.json
"""
import sys, os, csv, json, subprocess, numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, '/home/claude/la2a_ana')
from ana import load, sig
from wavio import read_wav

INFO = '/home/claude/la2a_data/info/renders.csv'
EXE = os.path.join(HERE, '..', 'build', 'FreeLA2Compare_artefacts', 'Release', 'FreeLA2Compare')
SR = 96000
ROWS = sorted(csv.DictReader(open(INFO)), key=lambda r: r['time'])

def plugin_settings(r):
    """Nebula knob settings -> FreeLA2 parameters (plug-in units) + stage mask."""
    id, en = r['id'], r['enabled']
    st = (1 if 'PRE' in en else 0) | (2 if 'COMP' in en else 0) | (4 if 'CLIP' in en else 0)
    k = dict(power=1, pushed=1 if 'PUSHED' in id else 0, drive=0.0, limit=1 if 'LIMIT' in id else 0,
             reduction=0.0, emphasis=0.0, gain=0.0, ahead=0, clipdrive=0.0, clipout=0.0, mix=100.0)
    pre_out_db, input_db = 0.0, 0.0
    skip = None
    for part in [s.strip() for s in r['settings'].split(';') if '=' in s]:
        key, v = part.split('='); v = float(v)
        if key == 'COMP.REDUCTION': k['reduction'] = 100 * v
        elif key == 'COMP.EMPHASIS': k['emphasis'] = 200 * v - 100
        elif key == 'COMP.INPUT':
            if v <= 0: skip = 'INPUT 0 = mute'
            input_db = 20 * np.log10(max(v, 1e-6) / 0.250594)
        elif key == 'COMP.OUTPUT': k['gain'] = 24.9 * v
        elif key == 'COMP.DRY': skip = 'DRY (replaced by MIX)' if v > 0 else None
        elif key == 'COMP.AHEAD': pass                      # no effect in the Nebula set; FreeLA2's AHEAD stays 0
        elif key == 'PRE.DRV': k['drive'] = 60 * (v - 0.5)
        elif key == 'PRE.OUT':
            if v <= 0: skip = 'OUT 0 = mute'
            pre_out_db = 20 * np.log10(max(2 * v, 1e-6))
        elif key == 'CLIP.IN': k['clipdrive'] = 40 * v
        elif key == 'CLIP.OUT': k['clipout'] = 10 * v + 1.13 * v * (1 - v)
    k['level'] = float(np.clip(pre_out_db + input_db, -18, 18))
    if st == 0: skip = 'dry reference'
    return st, k, skip

def run(tag, oslive=0, filt=0, phase=0):
    d = '/tmp/cmp/%s' % tag; os.makedirs(d + '/out', exist_ok=True); os.makedirs('/tmp/cmp/sig', exist_ok=True)
    jobs = ['global oslive=%d filter=%d phase=%d rate=%d' % (oslive, filt, phase, SR)]
    meta = []
    for r in ROWS:
        sec = r['section']; sp = '/tmp/cmp/sig/%s.f32' % sec
        x = sig(sec)
        if not os.path.exists(sp): np.ascontiguousarray(x, dtype=np.float32).tofile(sp)
        st, k, skip = plugin_settings(r)
        op = '%s/out/%s.f32' % (d, r['id'])
        jobs.append('%s %s %d stages=%d ' % (sp, op, len(x), st) + ' '.join('%s=%g' % kv for kv in k.items()))
        meta.append(dict(id=r['id'], stage=r['stage'], sec=sec, stages=st, settings=k, skip=skip, n=len(x)))
    open(d + '/jobs.txt', 'w').write('\n'.join(jobs) + '\n')
    json.dump(meta, open(d + '/meta.json', 'w'))
    out = subprocess.run([EXE, d + '/jobs.txt'], capture_output=True, text=True)
    print(out.stdout.strip(), out.stderr.strip()[-500:])

def env_db(x, w=960):
    n = len(x) // w; p = np.mean((x[:n * w] ** 2).reshape(n, w, -1), axis=(1, 2))
    return 10 * np.log10(p + 1e-20)

def metrics(tag, ref_tag=None):
    d = '/tmp/cmp/%s' % tag; meta = json.load(open(d + '/meta.json')); res = []
    for m in meta:
        if m['skip']: continue
        y = np.fromfile('%s/out/%s.f32' % (d, m['id']), dtype=np.float32).reshape(-1, 2).astype(np.float64)
        ref = load(m['id'])
        n = min(len(y), len(ref)); a = SR                          # skip the first second
        yy, rr = y[a:n], ref[a:n]
        e = yy - rr
        null = 10 * np.log10(np.sum(e ** 2) / np.sum(rr ** 2))
        xx = sig(m['sec'])[a:n]
        ey, er, ex = env_db(yy, 4800), env_db(rr, 4800), env_db(xx, 4800); mask = (er > -70) & (ex > -90)
        de = (ey - er)[mask]
        tail = (ex < -150) & (er > -90)
        tail_ref = float(np.max(er[tail])) if np.any(tail) else -200.0; tail_plug = float(np.max(ey[tail])) if np.any(tail) else -200.0
        res.append(dict(id=m['id'], stage=m['stage'], sec=m['sec'], null=null, env_rms=float(np.sqrt(np.mean(de ** 2))),
                        env_p95=float(np.percentile(np.abs(de), 95)), env_max=float(np.max(np.abs(de))), env_mean=float(np.mean(de)), tail_ref=tail_ref, tail_plug=tail_plug))
    json.dump(res, open(d + '/metrics.json', 'w'), indent=0)
    return res

if __name__ == '__main__':
    tag = sys.argv[1]; os_ = int(sys.argv[2]) if len(sys.argv) > 2 else 0; f = int(sys.argv[3]) if len(sys.argv) > 3 else 0
    ph = int(sys.argv[4]) if len(sys.argv) > 4 else 0
    run(tag, os_, f, ph)
    res = metrics(tag)
    for r in res: print('%-34s null %6.1f  level rms %5.2f p95 %5.2f max %5.2f mean %+5.2f  tail neb %6.1f plug %6.1f' % (r['id'], r['null'], r['env_rms'], r['env_p95'], r['env_max'], r['env_mean'], r['tail_ref'], r['tail_plug']))
