import sys, json, numpy as np
sys.path.insert(0, '/home/claude/freela2/fit'); sys.path.insert(0, '/home/claude/la2a_ana')
import la2
from ana import load, sig, R
from meas import steps_arr, UP, DN
m = np.array(json.load(open('../docs/freela2_model_v0.json'))['values'])
def kw_for(id):
    st = R[id]['settings']; en = R[id]['enabled']; k = dict(preOn=int('PRE' in en), compOn=int('COMP' in en), clipOn=int('CLIP' in en),
        pushed=int('PUSHED' in id), limit=int('LIMIT' in id), filterMode=0, filtersForce=0)
    for part in [s.strip() for s in st.split(';') if '=' in s]:
        key, v = part.split('='); v = float(v)
        if key == 'COMP.REDUCTION': k['reduction'] = 100 * v
        elif key == 'COMP.EMPHASIS': k['emphasis'] = 200 * v - 100
        elif key == 'COMP.INPUT': k['inputDb'] = 20 * np.log10(max(v, 1e-6) / 0.250594)
        elif key == 'COMP.OUTPUT': k['gainDb'] = 24.9 * v
        elif key == 'PRE.DRV': k['drive'] = 60 * (v - 0.5)
        elif key == 'PRE.OUT': k['preOutDb'] = 20 * np.log10(max(2 * v, 1e-6))
        elif key == 'CLIP.IN': k['clipIn'] = 40 * v
        elif key == 'CLIP.OUT': k['clipOutDb'] = 10 * v + 1.13 * v * (1 - v)
    return k
def null(id, skip=1.0):
    sec = id.split('__')[1]; x = sig(sec); k = kw_for(id); y = la2.run(m, x, **k); ref = load(id)
    lat = 1 if k.get('preOn', 1) else 0
    if lat: y = np.vstack([y[lat:], np.zeros((lat, 2))])
    n = min(len(y), len(ref)); a = int(skip * 96000)
    d = y[a:n] - ref[a:n]; return 20 * np.log10(np.sqrt(np.mean(d ** 2)) / np.sqrt(np.mean(ref[a:n] ** 2)))
if __name__ == '__main__':
    for id in sys.argv[1:]: print('%-28s null %6.1f dB   %s' % (id, null(id), kw_for(id)))
