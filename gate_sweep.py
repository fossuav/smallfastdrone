"""Sweep candidate witness gates for the SRCF vote against a field log.

can_vote asks whether the vehicle is on the GPS lane with a live receiver,
and never whether the flow lane it is being compared against is measuring
anything. Log 356 flew 250s of acro with the rangefinder returning for 12%
of the time, and the unaided flow lane dead reckoned a velocity that
disagreed with a healthy GPS lane by up to 34 m/s.

For each candidate this reports the longest unbroken run of "VD over its
gate AND the candidate still permits the vote". A run reaching
SRCF_CNF_TIME*10 samples still latches a spoof, so a candidate has to get
the run below that to be worth anything on its own.

Velocity only, deliberately. PR cannot be used on a log whose lanes are
far apart: pos_div is a float, so at the 6.6e6 m of log 356 its spacing is
0.5 m and every PR sample is a multiple of the resulting 0.25 m/s quantum.

Usage: gate_sweep.py <log.bin> [from_s] [to_s]

SRCF_VEL_THR and SRCF_CNF_TIME have both moved over the feature's life, so
set VEL_THR and CNF_TIME in the environment to read an older log against
what it actually flew: log 329 is 0.8 and 2.0, log 356 is 1.6 and 1.5.
"""
import os
import sys
from pymavlink import mavutil

VEL_THR = float(os.environ.get('VEL_THR', 1.6))
CNF_SAMPLES = int(float(os.environ.get('CNF_TIME', 1.5)) * 10)


def load(path):
    """SRCF samples plus the rangefinder health they have to be judged with"""
    m = mavutil.mavlink_connection(path)
    srcf, rfnd = [], []
    t0 = None
    while True:
        msg = m.recv_match(type=['SRCF', 'RFND'])
        if msg is None:
            break
        t = msg.TimeUS / 1e6
        if t0 is None:
            t0 = t
        t -= t0
        if msg.get_type() == 'SRCF':
            srcf.append((t, msg.VD, msg.FlwU))
        elif msg.Instance == 0:
            rfnd.append((t, 1 if msg.Stat == 4 else 0))
    return srcf, rfnd


def sample_at(series, t, max_age):
    """last value at or before t, None if there is none within max_age"""
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


def longest_run(srcf, pred, lo, hi):
    run = best = n_over = n_vote = 0
    for (t, vd, flw) in srcf:
        if t < lo or t > hi:
            continue
        if vd <= VEL_THR:
            run = 0
            continue
        n_over += 1
        if pred(t, flw):
            n_vote += 1
            run += 1
            best = max(best, run)
        else:
            run = 0
    return best, n_over, n_vote


def main():
    path = sys.argv[1]
    lo = float(sys.argv[2]) if len(sys.argv) > 2 else 0.0
    hi = float(sys.argv[3]) if len(sys.argv) > 3 else 1e9
    srcf, rfnd = load(path)

    def no_test(t, flw):
        return True

    def usable(t, flw):
        return flw == 1

    def usable_and_range(t, flw):
        # gndOffsetValid, which horiz_pos_rel rides on, survives 5s past the
        # last range, so flow_usable alone keeps voting while the lane coasts
        # on a stale terrain offset with no height to scale flow by
        if flw != 1:
            return False
        r = sample_at(rfnd, t, 0.5)
        return r is not None and r[1] == 1

    def make_held(hold_s):
        # the lane restarted aiding 11 times in log 356 and is not a witness
        # while it is still settling after a reset
        def held(t, flw):
            if flw != 1:
                return False
            for (ts, vds, flws) in reversed(srcf):
                if ts > t:
                    continue
                if t - ts > hold_s:
                    return True
                if flws != 1:
                    return False
            return False
        return held

    rows = [
        ("today: no witness test", no_test),
        ("flow_usable", usable),
        ("flow_usable and rangefinder returning", usable_and_range),
        ("flow_usable held 1s", make_held(1.0)),
        ("flow_usable held 2s", make_held(2.0)),
        ("flow_usable held 5s", make_held(5.0)),
        ("flow_usable held 10s", make_held(10.0)),
    ]

    print("VD gate %.1f, %d samples latch a spoof" % (VEL_THR, CNF_SAMPLES))
    print("%-40s %10s %10s %14s" % ("candidate", "over-gate", "voting", "longest run"))
    for name, pred in rows:
        best, n_over, n_vote = longest_run(srcf, pred, lo, hi)
        print("%-40s %10d %10d %6d (%5.1fs)%s" %
              (name, n_over, n_vote, best, best * 0.1,
               "" if best < CNF_SAMPLES else "  LATCHES"))


if __name__ == '__main__':
    main()
