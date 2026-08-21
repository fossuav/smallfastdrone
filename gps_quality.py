"""What separates a repeater/indoor fix from an open-sky one?

Dumps the receiver-reported quality channels over the armed window of
each log, so a candidate discriminator can be chosen from data rather
than from intuition.
"""
import sys
from pymavlink import mavutil


def pct(v, q):
    if not v:
        return float('nan')
    v = sorted(v)
    return v[min(len(v) - 1, int(q * len(v)))]


print("%-9s | %-11s | %-11s | %-13s | %-13s | %-11s | %s"
      % ("log", "NSats p5/p50", "Status p50", "HDop p50/p95", "HAcc p50/p95",
         "VAcc p50", "magI/magQ p50"))
print("-" * 106)
for path in sys.argv[1:]:
    m = mavutil.mavlink_connection(path)
    t_arm = t_dis = None
    sats, stat, hdop, hacc, vacc, magi, magq = [], [], [], [], [], [], []
    pend = []
    while True:
        msg = m.recv_match(type=['EV', 'GPS', 'GPA', 'UBX2'])
        if msg is None:
            break
        t = msg.TimeUS / 1e6
        ty = msg.get_type()
        if ty == 'EV':
            if msg.Id == 10 and t_arm is None:
                t_arm = t
            elif msg.Id == 11 and t_arm is not None and t_dis is None:
                t_dis = t
            continue
        pend.append((t, ty, msg))
    if t_arm is None:
        print("%-9s | never armed" % path.split('/')[-1])
        continue
    t_dis = t_dis if t_dis else 1e9
    for t, ty, msg in pend:
        if not (t_arm <= t <= t_dis):
            continue
        if ty == 'GPS' and getattr(msg, 'I', 0) == 0:
            sats.append(msg.NSats)
            stat.append(msg.Status)
            hdop.append(msg.HDop)
        elif ty == 'GPA' and getattr(msg, 'I', 0) == 0:
            hacc.append(msg.HAcc)
            vacc.append(msg.VAcc)
        elif ty == 'UBX2' and getattr(msg, 'Instance', 0) == 0:
            magi.append(msg.magI)
            magq.append(msg.magQ)
    print("%-9s | %4.0f %6.0f | %11.0f | %5.2f %7.2f | %5.2f %7.2f | %11.2f | %s"
          % (path.split('/')[-1].replace('.bin', ''),
             pct(sats, .05), pct(sats, .5), pct(stat, .5),
             pct(hdop, .5), pct(hdop, .95), pct(hacc, .5), pct(hacc, .95),
             pct(vacc, .5),
             ("%.0f/%.0f" % (pct(magi, .5), pct(magq, .5))) if magi else "-"))
