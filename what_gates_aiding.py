#!/usr/bin/env python3
"""What stopped the flow lane aiding?

AID_RELATIVE demotes to AID_NONE on flowFusionTimeout: 5 s without
FuseOptFlow reaching its update (AP_NavEKF3_Control.cpp:313). Fusion needs
flowDataToFuse AND tiltOK (OptFlowFusion.cpp:85), and then has to pass the
innovation gate. So the 5 s window before each demote is the period in
which none of that happened, and its state says which term was binding.

Reports, per demote, over the preceding 5 s:
  tiltOK%   share of samples with c.z > DCM33FlowMin (0.71)
  rngOK%    share with a returning rangefinder
  NI        XKF5 innovation-rejection counter on the flow lane
  qual      flow quality

Usage: what_gates_aiding.py <log.bin>
"""
import math
import sys
from pymavlink import mavutil

DCM33 = 0.71
WIN = 5.0


def main():
    path = sys.argv[1]
    m = mavutil.mavlink_connection(path)
    drops, att, rfnd, xkf5, of = [], [], [], [], []
    t0 = None
    while True:
        msg = m.recv_match(type=['MSG', 'ATT', 'RFND', 'XKF5', 'OF'])
        if msg is None:
            break
        t = msg.TimeUS / 1e6
        if t0 is None:
            t0 = t
        t -= t0
        ty = msg.get_type()
        if ty == 'MSG':
            if 'stopped aiding' in msg.Message:
                drops.append(t)
        elif ty == 'ATT':
            att.append((t, math.cos(math.radians(msg.Roll)) * math.cos(math.radians(msg.Pitch))))
        elif ty == 'RFND' and msg.Instance == 0:
            rfnd.append((t, msg.Stat == 4))
        elif ty == 'XKF5':
            xkf5.append((t, msg.NI))
        elif ty == 'OF':
            of.append((t, msg.Qual))

    def win(series, a, b):
        return [v for (t, v) in series if a <= t < b]

    print("%s: %d flow-lane aiding drops\n" % (path.split('/')[-1], len(drops)))
    if not drops:
        return 0
    print("state over the 5s of no-fusion before each drop")
    print("%8s %8s %8s %10s %8s" % ("t (s)", "tiltOK%", "rngOK%", "NI p50/max", "qual p50"))
    agg_tilt, agg_rng = [], []
    for d in drops:
        cz = win(att, d - WIN, d)
        rg = win(rfnd, d - WIN, d)
        ni = win(xkf5, d - WIN, d)
        q = win(of, d - WIN, d)
        tp = 100.0 * sum(1 for v in cz if v > DCM33) / len(cz) if cz else float('nan')
        rp = 100.0 * sum(1 for v in rg if v) / len(rg) if rg else float('nan')
        agg_tilt.append(tp)
        agg_rng.append(rp)
        ni_s = sorted(ni)
        q_s = sorted(q)
        print("%8.1f %7.0f%% %7.0f%% %10s %8s"
              % (d, tp, rp,
                 ("%.0f/%.0f" % (ni_s[len(ni_s) // 2], ni_s[-1])) if ni_s else "-",
                 ("%.0f" % q_s[len(q_s) // 2]) if q_s else "-"))
    print("\nmean over all drops: tiltOK %.0f%%   rngOK %.0f%%"
          % (sum(agg_tilt) / len(agg_tilt), sum(agg_rng) / len(agg_rng)))
    print("\ntiltOK high with rngOK low  -> the height reference gated it, not lean")
    print("tiltOK low                  -> lean gated it (and the sweep is circular)")
    return 0


if __name__ == '__main__':
    sys.exit(main())
