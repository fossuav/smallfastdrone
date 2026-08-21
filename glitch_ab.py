"""Replay A/B: does EK3_GLITCH_RAD explain the GPS lane's velocity excursion?

This airframe flies EK3_GLITCH_RAD = 0, which selects the bounded-update
path: a failed innovation test is not rejected and does not reset, the
innovation variance is scaled instead and the state crawls toward GPS.
On a wandering fix that could put position error into the velocity state,
which is what log 350 shows during the reversal.
"""
import math, os, subprocess, sys
from pymavlink import mavutil

def gps_lane_speed(path, core, lo, hi):
    m = mavutil.mavlink_connection(path)
    out = []
    while True:
        msg = m.recv_match(type=['XKF1'])
        if msg is None:
            break
        t = msg.TimeUS/1e6
        if msg.C == core and lo <= t <= hi:
            out.append(math.hypot(msg.VN, msg.VE))
    return out

def replay(src, rad):
    r = subprocess.run(['build/sitl/tool/Replay', '--force-ekf3',
                        '--parm', 'EK3_GLITCH_RAD=%d' % rad, src],
                       capture_output=True, text=True, timeout=900)
    if r.returncode != 0:
        return None
    logs = sorted((os.path.join('logs', f) for f in os.listdir('logs')
                   if f.endswith('.BIN')), key=os.path.getmtime)
    return logs[-1]

src, lo, hi = sys.argv[1], float(sys.argv[2]), float(sys.argv[3])
print("GPS lane horizontal speed over %.0f-%.0fs, replayed core 100" % (lo, hi))
print("%-22s %8s %8s %8s" % ("EK3_GLITCH_RAD", "p50", "p95", "max"))
print("-"*50)
for rad in (0, 25):
    out = replay(src, rad)
    if out is None:
        print("%-22d replay failed" % rad); continue
    v = sorted(gps_lane_speed(out, 100, lo, hi))
    if not v:
        print("%-22d no samples" % rad); continue
    print("%-22d %8.2f %8.2f %8.2f"
          % (rad, v[len(v)//2], v[int(.95*len(v))], v[-1]))
