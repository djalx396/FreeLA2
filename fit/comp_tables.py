# Build the static GR tables (3 curves + theta/cpos per knob position) for one program.
import numpy as np, json
from scipy.interpolate import PchipInterpolator
U0, DU, NU = -40.0, 0.5, 201          # u grid: -40 .. +60 dB
UG = U0 + DU * np.arange(NU)
KNOB = {'comp': [0.41984, 0.56091, 0.75742], 'comp_LIMIT': [0.43960, 0.56515, 0.75358]}
def curve_from_points(L, gr, top_slope=None):
    """monotone GR(L) on the measured levels -> (theta = level at 3 dB GR, curve on UG)"""
    L = np.asarray(L, float); gr = np.maximum.accumulate(np.maximum(np.asarray(gr, float), 0))
    i = int(np.argmax(gr >= 3.0)); th = L[i - 1] + (3.0 - gr[i - 1]) / (gr[i] - gr[i - 1]) * (L[i] - L[i - 1])
    u = L - th
    f = PchipInterpolator(u, gr, extrapolate=False)
    c = f(UG)
    lo = UG < u[0]; c[lo] = 0.0
    hi = UG > u[-1]
    if top_slope is None: top_slope = (gr[-1] - gr[-5]) / (L[-1] - L[-5])
    c[hi] = gr[-1] + top_slope * (UG[hi] - u[-1])
    return th, np.maximum(c, 0)
def build_tables(curves_pts, knobs, scan, d_cal=-10.0):
    """curves_pts: 3 x (L, gr). scan: (r, GR at d_cal). Returns dict with curves, theta(101), cpos(101)."""
    th = []; C = []
    for L, g in curves_pts:
        t, c = curve_from_points(L, g); th.append(t); C.append(c)
    C = np.array(C)
    rg = np.linspace(0, 1, 101)
    cpos = np.interp(rg, knobs, [0, 1, 2])
    sr = PchipInterpolator(scan[:, 0], np.maximum.accumulate(scan[:, 1]))
    target = sr(rg)
    def gr_at(theta, cp, d):
        ci = min(1, int(cp)); cf = cp - ci; u = d - theta
        return (1 - cf) * np.interp(u, UG, C[ci]) + cf * np.interp(u, UG, C[ci + 1])
    theta = np.zeros(101)
    for i, (r, cp, tg) in enumerate(zip(rg, cpos, target)):
        if tg < 0.3: theta[i] = np.nan; continue
        lo, hi = -80.0, 40.0                  # GR decreases as theta rises
        for _ in range(60):
            mid = 0.5 * (lo + hi)
            if gr_at(mid, cp, d_cal) > tg: lo = mid
            else: hi = mid
        theta[i] = 0.5 * (lo + hi)
    ok = ~np.isnan(theta)
    first = int(np.argmax(ok))
    slope = (theta[first + 10] - theta[first]) / (rg[first + 10] - rg[first])
    for i in range(first): theta[i] = theta[first] + slope * (rg[i] - rg[first])
    return dict(u0=U0, du=DU, curves=[list(c) for c in C], theta=list(theta), cpos=list(cpos), th_meas=th)
