"""GPS-lane position innovation, gated on the flow lane saying we are still.

A manoeuvring vehicle grows the GPS lane's position innovation through
sensor lag alone, so innovation on its own cannot separate a walking fix
from honest flying. The flow lane has no such lag - camera and rangefinder
are effectively instant - so it can say whether the vehicle is actually
moving. Innovation growing while flow says stationary is a fix walking
away from reality, and it is visible earlier than the velocity state that
SRCF currently watches.
"""
import math
import sys
from pymavlink import mavutil

STILL_MS = 0.5          # flow-lane speed under this counts as holding station


def load(path):
    m = mavutil.mavlink_connection(path)
    innov, flow, ev, att = [], [], [], []
    while True:
        msg = m.recv_match(type=['XKF3', 'XKF1', 'EV', 'ATT'])
        if msg is None:
            break
        t = msg.TimeUS / 1e6
        ty = msg.get_type()
        if ty == 'XKF3' and msg.C == 0:
            innov.append((t, math.hypot(msg.IPN, msg.IPE)))
        elif ty == 'XKF1' and msg.C == 1:
            flow.append((t, math.hypot(msg.VN, msg.VE)))
        elif ty == 'EV':
            ev.append((t, msg.Id))
        elif ty == 'ATT':
            att.append((t, max(abs(msg.Roll), abs(msg.Pitch))))
    return innov, flow, ev, att


def near(series, t, tol=0.5):
    best = None
    for s in series:
        d = abs(s[0] - t)
        if best is None or d < best[0]:
            best = (d, s[1])
        elif s[0] > t + tol:
            break
    return best[1] if best and best[0] <= tol else None


def series(path):
    innov, flow, ev, att = load(path)
    t_arm = next((t for t, i in ev if i == 10), None)
    if t_arm is None:
        return []
    t_end = min([t for t, i in ev if i in (11, 18, 54) and t > t_arm] or [1e9])
    out = []
    for t, ip in innov:
        if not (t_arm <= t <= t_end):
            continue
        a = near(att, t)
        v = near(flow, t)
        if a is None or v is None or a > 45.0:
            continue
        out.append((t, ip, v))
    return out


if __name__ == '__main__':
    print("GPS-lane position innovation while the flow lane reports under %.1f m/s" % STILL_MS)
    print("%-9s %-18s %8s | %-22s" % ("log", "environment", "samples", "innov p50/p95/max"))
    print("-" * 66)
    env = {332: "open sky", 333: "open sky", 336: "open sky", 337: "open sky",
           346: "repeater, CRASHED", 347: "repeater", 348: "indoor then out",
           349: "repeater", 350: "repeater, tripped"}
    for L in (332, 333, 336, 337, 347, 348, 349, 350, 346):
        s = [x for x in series('srcf_logs/log%d.bin' % L) if x[2] < STILL_MS]
        if not s:
            print("%-9s %-18s %8s" % ("log%d" % L, env[L], "none"))
            continue
        v = sorted(x[1] for x in s)
        print("%-9s %-18s %8d | %6.2f %6.2f %6.2f"
              % ("log%d" % L, env[L], len(v), v[len(v)//2], v[int(.95*len(v))], v[-1]))
