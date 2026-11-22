# Python wrapper around the FreeLA2 engine (libla2.so) for fitting and checks.
import ctypes, numpy as np, os
_lib = ctypes.CDLL(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'libla2.so'))
D = ctypes.POINTER(ctypes.c_double)
_lib.la2_new.restype = ctypes.c_void_p; _lib.la2_new.argtypes = [D, ctypes.c_int]
_lib.la2_free.argtypes = [ctypes.c_void_p]
_lib.la2_prepare.argtypes = [ctypes.c_void_p, ctypes.c_double, ctypes.c_int, D]
_lib.la2_update.argtypes = [ctypes.c_void_p, D]
_lib.la2_reset.argtypes = [ctypes.c_void_p]
_lib.la2_process.argtypes = [ctypes.c_void_p, D, D, ctypes.c_int, D]
_lib.la2_filter_delay.restype = ctypes.c_double; _lib.la2_filter_delay.argtypes = [ctypes.c_void_p]
MSIZE = 64 + 2 * 830 + 24
# model layout (mirrors LA2Engine.h)
lfF, lfQ, hfF, hfQ, k2, k3, h2ShelfF, h2ShelfDb, h2HpF, envRelMs, g2Tab = range(11)
LF1FP, LF1QP, LF2FZ, LF2QZ, LF2FP, LF2QP = 34, 35, 36, 37, 38, 39
PRETRIM = 43
HFTAB = 64 + 2 * 830
clipC, clipN = 40, 41
EMPH = 48
PROG0, PSIZE = 64, 830
P = dict(tauM=0, tauP=1, beta=2, w1=3, w2=4, ta1=5, E=6, pexp=7, ta2=8, ta3=9, tr1=10, tr2=11, tr3=12, detOff=13,
         cap2=14, cap3=15, lfTr=16, qRel=17, d3a=18, d3p=19, u0=20, du=21, nu=22, curves=23)
NUMAX = 201
THETA = 23 + 3 * NUMAX
CPOS = THETA + 101
D3CAP, D3FC = CPOS + 101, CPOS + 102
SKEYS = ['preOn','compOn','clipOn','pushed','drive','preOutDb','limit','inputDb','reduction','emphasis','gainDb',
         'aheadMs','clipIn','clipOutDb','mix','filterMode','filtersForce']
DEFAULT = dict(preOn=1, compOn=1, clipOn=1, pushed=0, drive=0, preOutDb=0, limit=0, inputDb=0, reduction=0, emphasis=0,
               gainDb=0, aheadMs=0, clipIn=0, clipOutDb=0, mix=1, filterMode=0, filtersForce=0)   # filterMode 0 OFF 1 CLEAN 2 AIR
def settings(**kw):
    d = dict(DEFAULT); d.update(kw); return np.array([float(d[k]) for k in SKEYS])
def pidx(limit, name): return PROG0 + (PSIZE if limit else 0) + (P[name] if isinstance(name, str) else name)

def run(model, x, fs=96000, factor=1, gr=False, **kw):
    """x: (n,2) float array at fs*factor. Returns output (and GR trace)."""
    m = np.ascontiguousarray(model, dtype=np.float64)
    e = _lib.la2_new(m.ctypes.data_as(D), len(m))
    s = settings(**kw)
    _lib.la2_prepare(e, fs, factor, s.ctypes.data_as(D))
    L = np.ascontiguousarray(x[:, 0], dtype=np.float64).copy()
    R = np.ascontiguousarray(x[:, 1] if x.shape[1] > 1 else x[:, 0], dtype=np.float64).copy()
    g = np.zeros(len(L))
    _lib.la2_process(e, L.ctypes.data_as(D), R.ctypes.data_as(D), len(L), g.ctypes.data_as(D))
    fd = _lib.la2_filter_delay(e)
    _lib.la2_free(e)
    y = np.stack([L, R], 1)
    return (y, g) if gr else y

def run_seq(model, xs, fs=96000, factor=1, **kw):
    """Process several signals back to back in one engine (state carries over, like REAPER renders)."""
    m = np.ascontiguousarray(model, dtype=np.float64)
    e = _lib.la2_new(m.ctypes.data_as(D), len(m))
    s = settings(**kw)
    _lib.la2_prepare(e, fs, factor, s.ctypes.data_as(D))
    outs = []
    for x in xs:
        L = np.ascontiguousarray(x[:, 0], dtype=np.float64).copy(); R = np.ascontiguousarray(x[:, 1], dtype=np.float64).copy()
        g = np.zeros(len(L))
        _lib.la2_process(e, L.ctypes.data_as(D), R.ctypes.data_as(D), len(L), g.ctypes.data_as(D))
        outs.append((np.stack([L, R], 1), g))
    _lib.la2_free(e)
    return outs
