"""Replay a proposed origin-free GPS cross-check over existing logs.

Horizontal: over a sliding window, compare the VECTOR displacement of the
GPS lane (XKF1 C=0) with that of the flow lane (C=1).  Frame-free: a
constant offset between the lanes cancels, so it needs no origin.

Vertical: compare the change in reported GPS altitude with the change in
baro altitude over the same window.  Also frame-free, and GPS height is
not fused at all with EK3_SRC*_POSZ=1, so it is a spare channel.
"""
import math
import sys
from pymavlink import mavutil

import os
WIN_S = float(os.environ.get('WIN_S', '20'))
STEP_S = 0.5
JUMP_M = 5.0        # per-sample lane jump = reset/alignment, breaks a window


def load(path):
    m = mavutil.mavlink_connection(path)
    lanes = {0: [], 1: []}
    gps, baro, arm, aid = [], [], [], []
    while True:
        msg = m.recv_match(type=['XKF1', 'XKF4', 'GPS', 'BARO', 'EV'])
        if msg is None:
            break
        t = msg.TimeUS / 1e6
        ty = msg.get_type()
        if ty == 'XKF1' and msg.C in (0, 1):
            lanes[msg.C].append((t, msg.PN, msg.PE))
        elif ty == 'XKF4' and msg.C == 0:
            aid.append((t, msg.AID))
        elif ty == 'GPS' and getattr(msg, 'I', 0) == 0:
            if msg.Status >= 3:
                gps.append((t, msg.Alt))
        elif ty == 'BARO' and getattr(msg, 'I', 0) == 0:
            baro.append((t, msg.Alt))
        elif ty == 'EV' and msg.Id in (10, 11):
            arm.append((t, msg.Id))
    return lanes, gps, baro, arm, aid


def armed_window(arm, lanes):
    t0 = t1 = None
    for t, i in arm:
        if i == 10 and t0 is None:
            t0 = t
        elif i == 11 and t0 is not None and t1 is None:
            t1 = t
    if t0 is None:
        return None
    if t1 is None:
        t1 = lanes[0][-1][0]
    return t0, t1


def sample(series, t):
    """nearest sample within 0.5 s, else None"""
    lo, hi = 0, len(series) - 1
    while lo < hi:
        mid = (lo + hi) // 2
        if series[mid][0] < t:
            lo = mid + 1
        else:
            hi = mid
    best = None
    for i in (lo - 1, lo, lo + 1):
        if 0 <= i < len(series):
            d = abs(series[i][0] - t)
            if best is None or d < best[0]:
                best = (d, series[i])
    if best is None or best[0] > 0.5:
        return None
    return best[1]


def breaks(series):
    """times of position jumps that are resets rather than motion"""
    out = []
    for i in range(1, len(series)):
        dt = series[i][0] - series[i - 1][0]
        if dt <= 0 or dt > 0.5:
            continue
        d = math.hypot(series[i][1] - series[i - 1][1], series[i][2] - series[i - 1][2])
        if d > JUMP_M:
            out.append(series[i][0])
    return out


def run(path):
    lanes, gps, baro, arm, aid = load(path)
    if not lanes[0] or not lanes[1]:
        return path, "no two-lane XKF1"
    aw = armed_window(arm, lanes)
    if aw is None:
        return path, "never armed"
    t0, t1 = aw
    t0 = max(t0, float(os.environ.get('T0', '-1e9')))
    t1 = min(t1, float(os.environ.get('T1', '1e9')))
    brk = breaks(lanes[0]) + breaks(lanes[1])

    hres, vres = [], []
    t = t0 + WIN_S
    while t <= t1:
        ta, tb = t - WIN_S, t
        if any(ta <= b <= tb for b in brk):
            t += STEP_S
            continue
        # the GPS lane must be absolutely aided across the whole window,
        # else both lanes are dead reckoning and disagreeing is expected
        span = [a for a in aid if ta <= a[0] <= tb]
        if not span or any(a[1] != 0 for a in span):
            t += STEP_S
            continue
        a0, b0 = sample(lanes[0], ta), sample(lanes[0], tb)
        a1, b1 = sample(lanes[1], ta), sample(lanes[1], tb)
        if a0 and b0 and a1 and b1:
            dgps = (b0[1] - a0[1], b0[2] - a0[2])
            dflw = (b1[1] - a1[1], b1[2] - a1[2])
            mism = math.hypot(dgps[0] - dflw[0], dgps[1] - dflw[1])
            hres.append((mism, math.hypot(*dflw)))
        ga, gb = sample(gps, ta), sample(gps, tb)
        ba, bb = sample(baro, ta), sample(baro, tb)
        if ga and gb and ba and bb:
            vres.append(abs((gb[1] - ga[1]) - (bb[1] - ba[1])))
        t += STEP_S
    return path, (hres, vres)


def pct(v, q):
    if not v:
        return float('nan')
    v = sorted(v)
    return v[min(len(v) - 1, int(q * len(v)))]


FLOOR_M = 2.0
print("%-10s %5s | %-19s | %-19s | %-16s"
      % ("log", "wins", "horiz mismatch max", "mism/max(trav,2m)", "vert p95/max"))
print("-" * 84)
for path in sys.argv[1:]:
    name, res = run(path)
    short = name.split('/')[-1]
    if isinstance(res, str):
        print("%-10s %s" % (short, res))
        continue
    hres, vres = res
    h = [x[0] for x in hres]
    r = [x[0] / max(x[1], FLOOR_M) for x in hres]
    if not h:
        print("%-10s %5d | no usable windows" % (short, 0))
        continue
    print("%-10s %5d | %5.1f %5.1f %5.1f m | %5.2f %5.2f %5.2f | %5.1f %5.1f m"
          % (short, len(h), pct(h, .5), pct(h, .95), max(h),
             pct(r, .5), pct(r, .95), max(r),
             pct(vres, .95), max(vres) if vres else float('nan')))
