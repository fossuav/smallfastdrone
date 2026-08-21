"""Replay logs at a range of EK3_CHECK_SCALE and report the GPS lane's
longest unbroken gpsGoodToAlign run.

The point is to find whether ArduPilot's own GPS checks, tightened by the
parameter that exists for it, separate a repeater fix from an open-sky one
without inventing new thresholds.
"""
import os
import subprocess
import sys
from pymavlink import mavutil

REPLAY = 'build/sitl/tool/Replay'


def longest_gta(path, core):
    m = mavutil.mavlink_connection(path)
    best = run = 0.0
    prev = None
    while True:
        msg = m.recv_match(type=['XKFS'])
        if msg is None:
            break
        if msg.C != core:
            continue
        t = msg.TimeUS / 1e6
        if msg.GPS_GTA and prev is not None:
            run += t - prev
            best = max(best, run)
        else:
            run = 0.0
        prev = t
    return best


def replay(src, scale, gps_check):
    cmd = [REPLAY, '--force-ekf3',
           '--parm', 'EK3_CHECK_SCALE=%d' % scale,
           '--parm', 'EK3_GPS_CHECK=%d' % gps_check,
           src]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=900)
    if r.returncode != 0:
        return None, (r.stderr or r.stdout)[-300:]
    # Replay writes the newest log into logs/
    logs = sorted((os.path.join('logs', f) for f in os.listdir('logs')
                   if f.endswith('.BIN')), key=os.path.getmtime)
    return logs[-1], None


if __name__ == '__main__':
    srcs = sys.argv[1:]
    print("longest unbroken gpsGoodToAlign on the GPS lane, seconds (replayed core 100)")
    header = "%-9s" % "log"
    combos = [(100, 31), (100, -1), (50, 31), (50, -1), (20, 31), (20, -1)]
    for sc, gc in combos:
        header += " %11s" % ("s%d/c%s" % (sc, 'all' if gc == -1 else gc))
    print(header)
    print("-" * len(header))
    for src in srcs:
        row = "%-9s" % src.split('/')[-1].replace('.bin', '')
        for sc, gc in combos:
            out, err = replay(src, sc, gc)
            if out is None:
                row += " %11s" % "ERR"
                sys.stderr.write("%s s%d c%d: %s\n" % (src, sc, gc, err))
                continue
            row += " %11.0f" % longest_gta(out, 100)
        print(row, flush=True)
