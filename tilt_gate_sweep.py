"""Replay a log at a range of DCM33FlowMin and score the flow lane against GPS.

DCM33FlowMin (AP_NavEKF3.h) is the tilt above which optical flow is not
fused at all - 0.71f, about 45 degrees. It is a compile-time const rather
than a parameter, so a sweep means a rebuild per value, the same shape as
the FLOW_AXIS_LOCKOUT_MS sweep in the optflow lockout topic.

Replay's usual limit is that it re-runs the estimator and not the vehicle,
so it cannot say what the aircraft would have done differently. That limit
is weak on a log flown in ACRO: the trajectory is pilot-commanded and the
estimate never feeds back into it, so changing the estimator cannot change
the flight path. On a log flown in a position mode the same sweep only
answers "what would the estimator have produced on this trajectory".

Read the reproduction column first. At the stock value the replayed flow
lane must match the flight's own, or nothing else in the table is evidence
about anything.

This edits a header in the working tree and rebuilds. It restores the
original on the way out, including after a failure, and verifies it.

Usage: tilt_gate_sweep.py <log.bin> [--values 0.71,0.60,0.50]
                          [--from 0] [--to 1e9]
"""
import argparse
import math
import os
import shutil
import subprocess
import sys
from pymavlink import mavutil

REPO = os.path.dirname(os.path.abspath(__file__))
HEADER = os.path.join(REPO, 'libraries/AP_NavEKF3/AP_NavEKF3.h')
REPLAY = os.path.join(REPO, 'build/sitl/tool/Replay')
STOCK = 0.71
# the flight's own lanes come through the replay log unchanged as C=0 and
# C=1; the replayed ones are the same cores offset by 100
GPS_LANE, FLOW_LANE, REPLAYED_FLOW = 0, 1, 101


def patch(value):
    with open(HEADER) as f:
        src = f.read()
    out, hits = [], 0
    for line in src.splitlines(True):
        if 'const float DCM33FlowMin' in line:
            head, _, tail = line.partition('=')
            comment = tail.split(';', 1)[1] if ';' in tail else '\n'
            out.append('%s= %.4ff;%s' % (head, value, comment))
            hits += 1
        else:
            out.append(line)
    if hits != 1:
        raise RuntimeError('expected 1 DCM33FlowMin definition, found %d' % hits)
    with open(HEADER, 'w') as f:
        f.write(''.join(out))


def build():
    r = subprocess.run(['./waf', 'replay'], cwd=REPO, capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError('build failed: %s' % (r.stderr or r.stdout)[-400:])


def replay(src):
    if os.path.isdir('logs'):
        shutil.rmtree('logs')
    r = subprocess.run([REPLAY, '--force-ekf3', src], capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError('replay failed: %s' % (r.stderr or r.stdout)[-400:])
    bins = [os.path.join('logs', f) for f in os.listdir('logs') if f.endswith('.BIN')]
    return max(bins, key=os.path.getmtime)


def load(path, lo, hi):
    m = mavutil.mavlink_connection(path)
    lane = {GPS_LANE: [], FLOW_LANE: [], REPLAYED_FLOW: []}
    t0 = None
    while True:
        msg = m.recv_match(type=['XKF1'])
        if msg is None:
            break
        t = msg.TimeUS / 1e6
        if t0 is None:
            t0 = t
        t -= t0
        if lo <= t <= hi and msg.C in lane:
            lane[msg.C].append((t, msg.VN, msg.VE, msg.PN, msg.PE))
    return lane


def at(series, t, age=0.15):
    lo, hi, best = 0, len(series) - 1, None
    while lo <= hi:
        mid = (lo + hi) // 2
        if series[mid][0] <= t:
            best = series[mid]
            lo = mid + 1
        else:
            hi = mid - 1
    return best if best is not None and (t - best[0]) <= age else None


def pct(v, p):
    s = sorted(v)
    return s[min(len(s) - 1, int(len(s) * p))]


def score(path, lo, hi):
    lane = load(path, lo, hi)
    repro, verr, drift = [], [], []
    base = prev = None
    flown = 0.0
    for (t, vn, ve, pn, pe) in lane[GPS_LANE]:
        r = at(lane[REPLAYED_FLOW], t)
        if r is None:
            continue
        f = at(lane[FLOW_LANE], t)
        if f is not None:
            repro.append(math.hypot(r[1] - f[1], r[2] - f[2]))
        verr.append(math.hypot(r[1] - vn, r[2] - ve))
        if base is None:
            base = (pn, pe, r[3], r[4])
        else:
            flown += math.hypot(pn - prev[0], pe - prev[1])
        prev = (pn, pe)
        drift.append(math.hypot((r[3] - base[2]) - (pn - base[0]),
                                (r[4] - base[3]) - (pe - base[1])))
    return dict(n=len(verr), flown=flown,
                repro=pct(repro, .95) if repro else float('nan'),
                v50=pct(verr, .5), v95=pct(verr, .95),
                vmean=sum(verr) / len(verr),
                dpeak=max(drift), dfinal=drift[-1],
                dpct=100 * max(drift) / flown if flown else float('nan'))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('log')
    ap.add_argument('--values', default='0.71,0.65,0.60,0.55,0.50')
    ap.add_argument('--from', dest='lo', type=float, default=0.0)
    ap.add_argument('--to', dest='hi', type=float, default=1e9)
    args = ap.parse_args()

    values = [float(v) for v in args.values.split(',')]
    backup = HEADER + '.sweep-backup'
    shutil.copy2(HEADER, backup)
    print("DCM33FlowMin sweep on %s, window %.0f-%.0fs" %
          (os.path.basename(args.log), args.lo, args.hi))
    print("truth is the GPS lane; %.2f is stock and its repro column is the check\n" % STOCK)
    print("%-16s %8s %8s %8s %9s %9s %8s" %
          ("DCM33FlowMin", "verr50", "verr95", "vmean", "driftpk", "% path", "repro95"))
    try:
        for v in values:
            patch(v)
            build()
            s = score(replay(args.log), args.lo, args.hi)
            print("%-16s %8.2f %8.2f %8.2f %9.1f %8.1f%% %8.2f" %
                  ("%.2f (%2.0f deg)%s" % (v, math.degrees(math.acos(min(1.0, v))),
                                           " *" if abs(v - STOCK) < 1e-9 else ""),
                   s['v50'], s['v95'], s['vmean'], s['dpeak'], s['dpct'], s['repro']))
    finally:
        shutil.move(backup, HEADER)
        build()
        with open(HEADER) as f:
            if ('%.2ff' % STOCK) not in f.read():
                print("\n*** HEADER NOT RESTORED - check %s ***" % HEADER, file=sys.stderr)
                return 1
        print("\nheader restored to %.2ff and rebuilt" % STOCK)
    return 0


if __name__ == '__main__':
    sys.exit(main())
