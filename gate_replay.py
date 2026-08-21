"""Replay first-fix handover gate variants over field logs.

A: shipped.  offset_ok = pos_div < NSIG * pos_sigma
B: A, or the fix is demonstrably good and has been for HOLD_GOOD_S.
   The reasoning: a large offset means either the fix or the datum is
   wrong, and a fix this receiver cannot produce under a repeater is
   evidence it is the datum.

Reports when each variant would first have commanded the handover.
"""
import os, sys
from pymavlink import mavutil

NSIG = 6.0
RECOV_S = 10.0
GOOD_SATS = int(os.environ.get('GOOD_SATS', '12'))
GOOD_HACC = float(os.environ.get('GOOD_HACC', '1.0'))
HOLD_GOOD_S = float(os.environ.get('HOLD_GOOD_S', '30'))


def load(path):
    m = mavutil.mavlink_connection(path)
    srcf, qual = [], []
    hacc = None
    while True:
        msg = m.recv_match(type=['SRCF', 'GPS', 'GPA'])
        if msg is None:
            break
        t = msg.TimeUS / 1e6
        ty = msg.get_type()
        if ty == 'GPA' and getattr(msg, 'I', 0) == 0:
            hacc = msg.HAcc
        elif ty == 'GPS' and getattr(msg, 'I', 0) == 0:
            qual.append((t, msg.Status, msg.NSats, hacc))
        elif ty == 'SRCF':
            # PSig and St postdate session 2; older logs simply have no
            # first-fix state to replay, but their fix quality is still usable
            try:
                srcf.append((t, msg.St, msg.PD, msg.PSig, msg.GpsL))
            except AttributeError:
                pass
    return srcf, qual


def near(series, t):
    best = None
    for s in series:
        d = abs(s[0] - t)
        if best is None or d < best[0]:
            best = (d, s)
        elif s[0] > t + 2:
            break
    return best[1] if best and best[0] <= 2.0 else None


def replay(path):
    srcf, qual = load(path)
    srcf = [s for s in srcf if s[1] == 3]          # FLOW_NO_GPS only
    if not srcf:
        return "never in FLOW_NO_GPS"
    t0 = srcf[0][0]
    good_since = None
    holds = {'A': None, 'B': None}
    fired = {'A': None, 'B': None}
    for t, st, pd, psig, gpsl in srcf:
        q = near(qual, t)
        fix_good = bool(q and q[1] >= 3 and q[2] >= GOOD_SATS
                        and q[3] is not None and q[3] <= GOOD_HACC)
        good_since = (good_since if fix_good and good_since is not None
                      else (t if fix_good else None))
        sustained = fix_good and good_since is not None and (t - good_since) >= HOLD_GOOD_S
        offset_ok = psig > 0 and pd < NSIG * psig
        for name, ok in (('A', offset_ok), ('B', offset_ok or sustained)):
            if fired[name] is not None:
                continue
            if gpsl == 1 and ok:
                holds[name] = t if holds[name] is None else holds[name]
                if t - holds[name] >= RECOV_S:
                    fired[name] = t
            else:
                holds[name] = None
    def fmt(v):
        return "%.0fs (t+%.0f)" % (v, v - t0) if v else "never"
    return "A %-16s  B %-16s" % (fmt(fired['A']), fmt(fired['B']))


print("gate: NSIG %.0f | good fix = >=%d sats, HAcc <=%.1f m, sustained %.0fs"
      % (NSIG, GOOD_SATS, GOOD_HACC, HOLD_GOOD_S))
print("-" * 62)
for p in sys.argv[1:]:
    print("%-9s %s" % (p.split('/')[-1].replace('.bin', ''), replay(p)))


def sustained_report(path):
    """longest unbroken run of 'good fix' at a range of bars"""
    srcf, qual = load(path)
    out = []
    for sats, hacc in ((8, 2.0), (10, 1.5), (12, 1.0), (14, 0.5)):
        best = run = 0
        prev = None
        for t, st, ns, ha in qual:
            ok = st >= 3 and ns >= sats and ha is not None and ha <= hacc
            if ok and prev is not None:
                run += t - prev
                best = max(best, run)
            else:
                run = 0
            prev = t
        out.append(best)
    return out


if os.environ.get('SUSTAINED'):
    print()
    print("longest unbroken 'good fix' run, seconds")
    print("%-9s %8s %8s %8s %8s" % ("log", ">=8/2.0", ">=10/1.5", ">=12/1.0", ">=14/0.5"))
    print("-" * 46)
    for p in sys.argv[1:]:
        r = sustained_report(p)
        print("%-9s %8.0f %8.0f %8.0f %8.0f"
              % (p.split('/')[-1].replace('.bin', ''), r[0], r[1], r[2], r[3]))
