# Wind Persistence

Learns the EKF horizontal wind estimate while flying with GPS, saves the
settled value to parameters that survive a reboot, and reinjects it into the
EKF on the next GPS-denied arm so dead reckoning starts with a realistic wind
state instead of zero.

This is useful for vehicles that take off without GPS (or that deliberately
switch to a GPS-denied EKF source set): without a wind seed the EKF assumes
still air and accumulates position error as it dead reckons through the real
wind. Seeding the last known wind reduces that drift.

The applet only writes wind to the EKF; it does not change any flight mode or
control behaviour. It requires the `ahrs:using_gps()`,
`ahrs:get_fly_forward()` and `ahrs:handle_external_wind_estimate()` bindings,
so the autopilot must have at least 1 MB of flash
(`AP_AHRS_POSITION_RESET_ENABLED`).

The EKF wind estimate is only meaningful in fixed-wing forward flight, so on a
VTOL airframe the applet learns wind only while `fly_forward` is set (i.e. once
transitioned to forward flight, not during hover).

# Parameters

WIND_OPT : master enable. 0 = applet idle, 1 = learn, save and reinject. Default 1.

WIND_SPD : persisted wind speed (m/s). Set automatically; 0 means nothing recorded yet.

WIND_DIR : persisted wind direction the wind is coming from (deg from true north). Set automatically.

WIND_SACC : persisted 1-sigma uncertainty of WIND_SPD (m/s). Set automatically.

WIND_DACC : persisted 1-sigma uncertainty of WIND_DIR (deg). Set automatically.

WIND_LAT : latitude (deg) where the wind was last recorded. Set automatically.

WIND_LON : longitude (deg) where the wind was last recorded. Set automatically.

WIND_DIST : if the current location is further than this (m) from WIND_LAT/LON, the saved wind is treated as stale and the warm-start is skipped. Set to 0 to disable the staleness check. Default 50000.

# How To Use

1. Copy `wind_persist.lua` to the `APM/scripts` directory on the SD card and set `SCR_ENABLE = 1`, then reboot.
2. Leave `WIND_OPT = 1`.
3. Fly a forward-flight leg with a healthy GPS fix. While armed, flying forward (`fly_forward` set), and the EKF is fusing GPS, the script samples the EKF wind at 1 Hz and, once it settles, saves it to `WIND_SPD`/`WIND_DIR` (and the location). A `Wind: saved ...` message is sent on each save. On a VTOL, transition to fixed-wing first; the script will not learn wind in hover.
4. On the next arm where the EKF is not fusing GPS (a GPS-denied takeoff, or a switch to a GPS-denied EKF source set before arming), the script pushes the saved wind into the EKF and reports `Wind: warm-started ...`.

Notes:

- The warm-start fires only on the disarmed-to-armed transition. The saved
  parameters persist across reboots, so the first arm after a reboot uses the
  last value learned before shutdown.
- Saving only happens while the EKF is actually fusing GPS, so a value learned
  during GPS-denied flight cannot overwrite a good one.
- If the vehicle has moved more than `WIND_DIST` from where the wind was
  recorded, the warm-start is skipped to avoid seeding a stale value after a
  long transit. Set `WIND_DIST = 0` to always use the saved value.
