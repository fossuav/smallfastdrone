"""Sustained-condition candidates for accepting a first fix.

GTA   : the EKF's own gpsGoodToAlign (XKFS.GPS_GTA), computed continuously
QUAL  : >=12 satellites and HAcc <= 1.0 m
BOTH  : GTA and QUAL together

Reports the longest unbroken run of each, per log.  A candidate is only
useful if the repeater flights cannot hold it for the required time and
the honest flight can.
"""
import sys
from pymavlink import mavutil


def runs(path):
    m = mavutil.mavlink_connection(path)
    gta, qual = [], []
    hacc = None
    while True:
        msg = m.recv_match(type=['XKFS', 'GPS', 'GPA'])
        if msg is None:
            break
        t = msg.TimeUS / 1e6
        ty = msg.get_type()
        if ty == 'XKFS' and msg.C == 0:
            gta.append((t, bool(msg.GPS_GTA)))
        elif ty == 'GPA' and getattr(msg, 'I', 0) == 0:
            hacc = msg.HAcc
        elif ty == 'GPS' and getattr(msg, 'I', 0) == 0:
            qual.append((t, msg.Status >= 3 and msg.NSats >= 12
                         and hacc is not None and hacc <= 1.0))

    def nearest(series, t):
        best = None
        for s in series:
            d = abs(s[0] - t)
            if best is None or d < best[0]:
                best = (d, s[1])
            elif s[0] > t + 2:
                break
        return best[1] if best and best[0] <= 2.0 else False

    out = []
    for name in ('GTA', 'QUAL', 'BOTH'):
        best = run = 0
        prev = None
        for t, g in gta:
            if name == 'GTA':
                ok = g
            elif name == 'QUAL':
                ok = nearest(qual, t)
            else:
                ok = g and nearest(qual, t)
            if ok and prev is not None:
                run += t - prev
                best = max(best, run)
            else:
                run = 0
            prev = t
        out.append(best)
    return out


print("longest unbroken run, seconds")
print("%-9s %9s %9s %9s" % ("log", "GTA", "QUAL", "BOTH"))
print("-" * 40)
for p in sys.argv[1:]:
    r = runs(p)
    print("%-9s %9.0f %9.0f %9.0f" % (p.split('/')[-1].replace('.bin', ''), *r))
