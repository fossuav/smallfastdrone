"""How well would flow-only navigation have done on a two-lane log?

Under per-core source sets the flow lane dead reckons whatever the primary
is, so a flight flown on GPS still contains a full flow-only trajectory to
score against truth. That answers two operator questions without flying
anything:

  1. Could this have been flown with no GPS and still had a usable RTL -
     how far does the flow lane's dead-reckoned displacement drift?
  2. Is flow plus rangefinder good enough to harden the estimate at speed
     and low altitude - how does the flow lane's velocity error behave
     against speed, height and tilt?

Truth is the GPS lane (core 0). Displacements are compared rather than
positions, so a lane whose frame is wrong - field log 356 armed with the
two 6623 km apart - still scores correctly.

Read the peak error, not the final one. A circuit brings the vehicle back
near its start and the scale error partly cancels, so the final figure
flatters the estimate; RTL fires when it fires.

Usage: flow_nav_viability.py <log.bin> [from_s] [to_s]
"""
import math
import sys
from pymavlink import mavutil

MIN_BIN = 20            # samples below this are reported, not summarised
PAIR_MAX_SKEW = 0.15    # s, matching the two lanes' XKF1 samples
AUX_MAX_AGE = 0.2       # s, attitude and rangefinder staleness


def load(path, lo, hi):
    m = mavutil.mavlink_connection(path)
    lane = {0: [], 1: []}
    att, rfnd, aid = [], [], []
    t0 = None
    while True:
        msg = m.recv_match(type=['XKF1', 'ATT', 'RFND', 'XKF4'])
        if msg is None:
            break
        t = msg.TimeUS / 1e6
        if t0 is None:
            t0 = t
        t -= t0
        if t < lo or t > hi:
            continue
        ty = msg.get_type()
        if ty == 'XKF1' and msg.C in (0, 1):
            lane[msg.C].append((t, msg.VN, msg.VE, msg.PN, msg.PE))
        elif ty == 'ATT':
            att.append((t, msg.Roll, msg.Pitch))
        elif ty == 'RFND' and msg.Instance == 0:
            rfnd.append((t, msg.Dist, msg.Stat))
        elif ty == 'XKF4' and msg.C == 1:
            aid.append((t, msg.AID))
    return lane, att, rfnd, aid


def at(series, t, max_age):
    """last sample at or before t, None if there is none within max_age"""
    lo, hi, best = 0, len(series) - 1, None
    while lo <= hi:
        mid = (lo + hi) // 2
        if series[mid][0] <= t:
            best = series[mid]
            lo = mid + 1
        else:
            hi = mid - 1
    if best is None or (t - best[0]) > max_age:
        return None
    return best


def pct(v, p):
    s = sorted(v)
    return s[min(len(s) - 1, int(len(s) * p))]


def main():
    path = sys.argv[1]
    lo = float(sys.argv[2]) if len(sys.argv) > 2 else 0.0
    hi = float(sys.argv[3]) if len(sys.argv) > 3 else 1e9
    lane, att, rfnd, aid = load(path, lo, hi)

    pairs = []
    for (t, vn0, ve0, pn0, pe0) in lane[0]:
        c1 = at(lane[1], t, PAIR_MAX_SKEW)
        if c1 is not None:
            pairs.append((t, vn0, ve0, pn0, pe0, c1[1], c1[2], c1[3], c1[4]))
    if len(pairs) < MIN_BIN:
        print("not enough paired two-lane samples (%d)" % len(pairs))
        return 1
    print("%s: %d paired samples, t=%.0f-%.0fs\n"
          % (path.split('/')[-1], len(pairs), pairs[0][0], pairs[-1][0]))

    # ---- Q1: dead-reckoned displacement against truth
    base = pairs[0]
    flown = 0.0
    errs = []
    steps = 0
    prev = None
    for p in pairs:
        if prev is not None:
            flown += math.hypot(p[3] - prev[3], p[4] - prev[4])
        dt = (p[3] - base[3], p[4] - base[4])
        df = (p[7] - base[7], p[8] - base[8])
        e = math.hypot(df[0] - dt[0], df[1] - dt[1])
        # a step this size in one sample is a reset, not drift
        if errs and (e - errs[-1][1]) > 5.0 and (p[0] - errs[-1][0]) < 0.5:
            steps += 1
        errs.append((p[0], e))
        prev = p
    peak = max(errs, key=lambda x: x[1])
    print("=== Q1: flow-lane displacement against GPS truth ===")
    print("path flown            %8.0f m over %.0f s" % (flown, pairs[-1][0] - pairs[0][0]))
    print("final error           %8.1f m  (%.1f%% of path)" % (errs[-1][1], 100 * errs[-1][1] / flown))
    print("peak error            %8.1f m  (%.1f%% of path) at t=%.0fs"
          % (peak[1], 100 * peak[1] / flown, peak[0]))
    print("position steps >5m    %8d   (resets, not drift)" % steps)

    # ---- Q2: velocity error against truth
    recs = []
    for p in pairs:
        a = at(att, p[0], AUX_MAX_AGE)
        r = at(rfnd, p[0], AUX_MAX_AGE)
        d = at(aid, p[0], 0.3)
        if a is None or r is None or d is None:
            continue
        cz = math.cos(math.radians(a[1])) * math.cos(math.radians(a[2]))
        recs.append(dict(
            spd=math.hypot(p[1], p[2]),
            verr=math.hypot(p[5] - p[1], p[6] - p[2]),
            tilt=math.degrees(math.acos(max(0.05, cz))),
            rng=r[1], rng_ok=(r[2] == 4),
            aiding=(d[1] == 2)))       # AID_RELATIVE

    def report(label, sel):
        v = [r['verr'] for r in recs if sel(r)]
        s = [r['spd'] for r in recs if sel(r)]
        if len(v) < MIN_BIN:
            print("%-38s n=%5d  (too few)" % (label, len(v)))
            return
        print("%-38s n=%5d  speed p50 %5.1f   verr p50 %5.2f  p95 %6.2f  max %6.2f"
              % (label, len(v), pct(s, .5), pct(v, .5), pct(v, .95), max(v)))

    print("\n=== Q2: flow-lane velocity error against GPS truth (m/s) ===")
    report("aiding, any height", lambda r: r['aiding'])
    report("aiding AND rangefinder returning", lambda r: r['aiding'] and r['rng_ok'])
    ok = lambda r: r['aiding'] and r['rng_ok']          # noqa: E731
    print()
    for a, b in ((0, 5), (5, 10), (10, 15), (15, 20), (20, 35)):
        report("  speed %2d-%2d m/s" % (a, b),
               lambda r, a=a, b=b: ok(r) and a <= r['spd'] < b)
    print()
    for a, b in ((0, 15), (15, 30), (30, 45), (45, 90)):
        # past 45 deg AP_SurfaceDistance clamps its tilt correction at 0.707
        # and the height fed to the flow scaling is simply wrong
        report("  tilt %2d-%2d deg" % (a, b),
               lambda r, a=a, b=b: ok(r) and a <= r['tilt'] < b)
    print()
    for a, b in ((0, 5), (5, 10), (10, 15), (15, 30)):
        report("  height %2d-%2d m" % (a, b),
               lambda r, a=a, b=b: ok(r) and a <= r['rng'] < b)

    n = len(recs)
    n_rng = sum(1 for r in recs if r['rng_ok'])
    print("\nrangefinder returning for %d of %d samples (%.0f%%)"
          % (n_rng, n, 100.0 * n_rng / n))
    return 0


if __name__ == '__main__':
    sys.exit(main())
