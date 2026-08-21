"""Measure the benign cross-lane velocity divergence against candidate predictors.

The fixed SRCF_VEL_THR has to cover hover, an indoor stick reversal and
30 km/h cruise with one number, so it is wrong in at least two of them.
This pulls VD alongside the things that plausibly drive it - the vehicle's
own speed and its roll/pitch rate - so a threshold that tracks conditions
can be chosen from data rather than guessed.
"""
import math
import sys
from pymavlink import mavutil


def load(path):
    m = mavutil.mavlink_connection(path)
    srcf, lanes, gyro, rfnd, att, ev = [], {0: [], 1: []}, [], [], [], []
    n = 0
    while True:
        msg = m.recv_match(type=['SRCF', 'XKF1', 'IMU', 'RFND', 'ATT', 'EV'])
        if msg is None:
            break
        t = msg.TimeUS / 1e6
        ty = msg.get_type()
        if ty == 'SRCF':
            try:
                srcf.append((t, msg.St, msg.VD, msg.PR))
            except AttributeError:
                return None
        elif ty == 'XKF1' and msg.C in (0, 1):
            lanes[msg.C].append((t, math.hypot(msg.VN, msg.VE)))
        elif ty == 'IMU' and getattr(msg, 'I', 0) == 0:
            n += 1
            if n % 8 == 0:      # ~50Hz is plenty
                gyro.append((t, math.hypot(msg.GyrX, msg.GyrY), abs(msg.GyrZ)))
        elif ty == 'RFND' and getattr(msg, 'Instance', 0) == 0:
            rfnd.append((t, msg.Dist))
        elif ty == 'ATT':
            att.append((t, max(abs(msg.Roll), abs(msg.Pitch))))
        elif ty == 'EV':
            ev.append((t, msg.Id))
    return srcf, lanes, gyro, rfnd, att, ev


def near(series, t, tol=0.5):
    lo, hi = 0, len(series) - 1
    if hi < 0:
        return None
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
    return best[1] if best and best[0] <= tol else None


def samples(path):
    got = load(path)
    if got is None:
        return []
    srcf, lanes, gyro, rfnd, att, ev = got
    # armed window only: a benign envelope cannot include the ground or,
    # in log 346, the wall
    t_arm = next((t for t, i in ev if i == 10), None)
    # 54 is MOTORS_EMERGENCY_STOPPED: past it the airframe is falling and the
    # flow lane is resetting, which is not an envelope, it is an accident
    t_end = min([t for t, i in ev if i in (11, 18, 54) and t_arm and t > t_arm] or [1e9])
    if t_arm is None:
        return []
    out = []
    for t, st, vd, pr in srcf:
        if vd == 0.0 and pr == 0.0:
            continue                      # muted or divergence unavailable
        if not (t_arm <= t <= t_end):
            continue
        a = near(att, t)
        if a is None or a[1] > 45.0:      # upright flight only, not a tumble
            continue
        primary = 0 if st == 0 else 1
        v = near(lanes[primary], t)
        g = near(gyro, t)
        r = near(rfnd, t, tol=1.0)
        if v is None or g is None:
            continue
        out.append({'t': t, 'vd': vd, 'pr': abs(pr), 'spd': v[1],
                    'rot': g[1], 'yaw': g[2], 'hagl': r[1] if r else float('nan')})
    return out


if __name__ == '__main__':
    allsam = []
    print("%-9s %7s | %-18s | %-16s | %-16s"
          % ("log", "samples", "VD p50/p99/max", "speed p50/max", "rot p50/max rad/s"))
    print("-" * 78)
    for p in sys.argv[1:]:
        sam = samples(p)
        if not sam:
            print("%-9s %7s" % (p.split('/')[-1].replace('.bin', ''), "no usable SRCF"))
            continue
        allsam += [dict(s, log=p.split('/')[-1].replace('.bin', '')) for s in sam]
        vd = sorted(s['vd'] for s in sam)
        sp = sorted(s['spd'] for s in sam)
        ro = sorted(s['rot'] for s in sam)
        q = lambda v, f: v[min(len(v) - 1, int(f * len(v)))]
        print("%-9s %7d | %5.2f %5.2f %5.2f | %6.2f %6.2f  | %6.2f %6.2f"
              % (p.split('/')[-1].replace('.bin', ''), len(sam),
                 q(vd, .5), q(vd, .99), vd[-1], q(sp, .5), sp[-1], q(ro, .5), ro[-1]))

    print("\nworst benign VD in each (speed, roll/pitch rate) cell, all logs pooled")
    sb = [0, 1, 2, 4, 8, 99]
    rb = [0, 0.25, 0.5, 1.0, 9]
    print("%-14s" % "speed \\ rot", end="")
    for i in range(len(rb) - 1):
        print("%12s" % ("%.2f-%.2f" % (rb[i], rb[i + 1])), end="")
    print()
    for i in range(len(sb) - 1):
        print("%-14s" % ("%g-%g m/s" % (sb[i], sb[i + 1])), end="")
        for j in range(len(rb) - 1):
            cell = [s['vd'] for s in allsam
                    if sb[i] <= s['spd'] < sb[i + 1] and rb[j] <= s['rot'] < rb[j + 1]]
            print("%12s" % ("%.2f/%d" % (max(cell), len(cell)) if cell else "-"), end="")
        print()
