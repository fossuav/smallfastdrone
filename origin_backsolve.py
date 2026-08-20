"""Back-solve the true takeoff point from a good GPS fix and the flow lane.

The flow lane in AID_RELATIVE reports displacement from where it began
aiding, i.e. the takeoff point.  Subtract that from a trusted GPS fix and
what is left is where the vehicle actually took off - which is what the
recorded origin is supposed to be.
"""
import math, sys
from pymavlink import mavutil

path, t_lo, t_hi = sys.argv[1], float(sys.argv[2]), float(sys.argv[3])
m = mavutil.mavlink_connection(path)
flow, gps, parm = [], [], {}
while True:
    msg = m.recv_match(type=['XKF1', 'GPS', 'PARM'])
    if msg is None:
        break
    if msg.get_type() == 'PARM':
        if msg.Name.startswith('AHRS_ORIGIN'):
            parm[msg.Name] = msg.Value
        continue
    t = msg.TimeUS / 1e6
    if not (t_lo <= t <= t_hi):
        continue
    if msg.get_type() == 'XKF1' and msg.C == 1:
        flow.append((t, msg.PN, msg.PE))
    elif msg.get_type() == 'GPS' and getattr(msg, 'I', 0) == 0 and msg.Status >= 3:
        gps.append((t, msg.Lat, msg.Lng, msg.Alt, msg.NSats, msg.HDop))

if not flow or not gps:
    raise SystemExit("no data in window")

lat0 = sum(g[1] for g in gps) / len(gps)
lng0 = sum(g[2] for g in gps) / len(gps)
alt0 = sum(g[3] for g in gps) / len(gps)
pn = sum(f[1] for f in flow) / len(flow)
pe = sum(f[2] for f in flow) / len(flow)

MDEG = 111320.0
lat_take = lat0 - pn / MDEG
lng_take = lng0 - pe / (MDEG * math.cos(math.radians(lat0)))

print("window %.0f-%.0f s: %d GPS fixes (%d-%d sats), %d flow samples"
      % (t_lo, t_hi, len(gps), min(g[4] for g in gps), max(g[4] for g in gps), len(flow)))
print("mean GPS position      %.7f, %.7f  alt %.1f m" % (lat0, lng0, alt0))
print("flow displacement      N %+.1f m, E %+.1f m  (from takeoff)" % (pn, pe))
print("=> takeoff point       %.7f, %.7f" % (lat_take, lng_take))

ol, og, oa = parm.get('AHRS_ORIGIN_LAT'), parm.get('AHRS_ORIGIN_LON'), parm.get('AHRS_ORIGIN_ALT')
if ol is not None:
    dn = (ol - lat_take) * MDEG
    de = (og - lng_take) * MDEG * math.cos(math.radians(lat0))
    print("   recorded origin     %.7f, %.7f  alt %.1f m" % (ol, og, oa))
    print("   origin error        N %+.1f m, E %+.1f m  = %.1f m horizontal, %+.1f m vertical"
          % (dn, de, math.hypot(dn, de), oa - alt0))
