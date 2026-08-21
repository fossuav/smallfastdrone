"""Replay the vote integrator at different SRCF_CNF_TIME.

Switchover latency is almost entirely confirmation time - the commanded
switch itself lands in under 5 ms - so the question is how far it can be
shortened before benign flights start tripping.
"""
import sys
from pymavlink import mavutil

VEL_THR = 1.6
POSR_THR = 1.9
NSIGMA = 2.5


def load(path):
    m = mavutil.mavlink_connection(path)
    out, ev = [], []
    while True:
        msg = m.recv_match(type=['SRCF', 'EV'])
        if msg is None:
            break
        t = msg.TimeUS / 1e6
        if msg.get_type() == 'EV':
            ev.append((t, msg.Id))
            continue
        try:
            out.append((t, msg.VD, msg.PR, msg.VSig, msg.St))
        except AttributeError:
            return None, None
    return out, ev


def trips(rows, cnf, mute_s=5.0):
    """returns list of trip times under this confirmation window.

    Votes only accumulate while the GPS lane is primary - can_vote in the
    monitor - and the detectors are muted for mute_s after any lane change,
    so both have to be modelled or a benign flight looks like it trips.
    """
    vote_max = max(1, int(cnf * 10))
    vv = pv = 0
    out = []
    last_st = None
    last_switch = -1e9
    for t, vd, pr, vsig, st in rows:
        # The firmware votes, then switches state, then logs, so the tick that
        # completes a trip carries the new state. Judge can_vote on the state
        # this sample was voted under, not the one it was written with, or the
        # replay lands one tick short - it reproduces the logged peaks of 19,
        # 19 and 10 either way, and only the trip itself is lost.
        voting_st = last_st if last_st is not None else st
        if last_st is not None and st != last_st:
            last_switch = t
        last_st = st
        if voting_st != 0:                 # not flying on the GPS lane
            vv = pv = 0
            continue
        if t - last_switch < mute_s:
            continue
        if vd == 0.0 and pr == 0.0:
            continue                       # muted or unavailable
        sig = NSIGMA * vsig
        vg = max(VEL_THR, sig)
        pg = max(POSR_THR, sig)
        vv = min(vv + 1, vote_max) if vd > vg else max(0, vv - 1)
        pv = min(pv + 1, vote_max) if abs(pr) > pg else max(0, pv - 1)
        if vv >= vote_max or pv >= vote_max:
            out.append(t)
            vv = pv = 0                    # a trip switches lanes and resets
    return out


if __name__ == '__main__':
    cnfs = [0.5, 1.0, 1.5, 2.0, 3.0]
    print("trips per flight by SRCF_CNF_TIME (first trip time in brackets)")
    print("%-9s %-20s" % ("log", "kind"), end="")
    for c in cnfs:
        print("%14s" % ("%.1fs" % c), end="")
    print()
    print("-" * (30 + 14 * len(cnfs)))
    kind = {332: "outdoor benign", 333: "outdoor benign", 334: "outdoor benign",
            335: "outdoor benign", 336: "outdoor benign", 337: "outdoor benign",
            346: "indoor, crashed", 347: "indoor", 348: "indoor then out",
            349: "indoor, tripped", 350: "indoor, tripped", 352: "indoor, tripped"}
    for L in sorted(kind):
        rows, ev = load('srcf_logs/log%d.bin' % L)
        if rows is None:
            print("%-9s %-20s  no VSig in this log format" % ("log%d" % L, kind[L]))
            continue
        t_arm = next((t for t, i in ev if i == 10), None)
        t_end = min([t for t, i in ev if i in (11, 18, 54) and t_arm and t > t_arm] or [1e9])
        rows = [r for r in rows if t_arm and t_arm <= r[0] <= t_end]
        print("%-9s %-20s" % ("log%d" % L, kind[L]), end="")
        for c in cnfs:
            tr = trips(rows, c)
            print("%14s" % ("%d (%.0fs)" % (len(tr), tr[0]) if tr else "0"), end="")
        print()
