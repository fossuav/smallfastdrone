# Small Fast Drone Project

The canonical ArduPilot README can be found [here](https://github.com/ArduPilot/ardupilot/blob/master/README.md)

[![Test Copter](https://github.com/ArduPilot/ardupilot/workflows/test%20copter/badge.svg?branch=master)](https://github.com/ArduPilot/ardupilot/actions/workflows/test_sitl_copter.yml)

![SFD](sfd_logo2.png)

ArduPilot is the most advanced, full-featured, and reliable open source autopilot software available.
It has been under development since 2010 by a diverse team of professional engineers, computer scientists, and community contributors.
The autopilot software is capable of controlling almost any vehicle system imaginable, from conventional airplanes, quad planes, multi-rotors, and helicopters to rovers, boats, balance bots, and even submarines. It is continually being expanded to provide support for new emerging vehicle types.

However, the need to continously support all vehicle types constrains both flash usage and feature velocity. That might be fine if you are flying a 2 ton, $100k vehicle and absolute reliability is paramount - but for smaller vehicles these constraints can be prohibitive to forward progress - and yet at the same time you also don't want to be flying the master branch the whole time. For a while I have be maintaining branches off the latest stable branch of ArduPilot that also contain features - usually features that I have developed - that are either only available in master or as PRs. These features are particularly geared to the needs to smaller, faster copters - but are also can be applicable to any size of vehicle. Maintaining these
branches has become somewhat onerous, so I have instead started this new repo giving me greater flexibility in how I managed progress. The intent is:

- To be a derivative of the latest ArduPilot release (this branch tracks 4.7.x, currently 4.7.2)
- For all included features to be open source and eventually be available in either the main ArduPilot repository or one of the fossuav repositories
- For all additional features to have been flown
- For all additional features to be documented

## Included Features ##

The branch is based on ArduPilot 4.7.2. It also includes the features below,
grouped by function.

Entries marked **merged** have landed in ArduPilot master since they were picked
up here, so they will arrive in a future stable release; they are carried on this
branch because 4.7 predates them. Entries marked **in 4.7.2** are part of the
4.7.2 release this branch is based on. Everything else is work local to this repo.

### Optical Flow & AGL Kalman Filter ###

- IMU-aided AGL Filter for Optical Flow (https://github.com/ArduPilot/ardupilot/pull/32389) - **merged**
- SRTM Height Sign Fix for Flow Scaling (https://github.com/ArduPilot/ardupilot/pull/34360) - **in 4.7.2**

### EKF & Inertial Navigation ###

- Recorded Origin Fix (https://github.com/ArduPilot/ardupilot/pull/32469) - **in 4.7.2**
- EK3_MAG_CAL=7 Ground and In-flight Mode (https://github.com/ArduPilot/ardupilot/pull/32200) - **merged**
- EKF Zero Velocity Fusion (https://github.com/ArduPilot/ardupilot/pull/32396) - **merged**
- Zero Velocity Fusion Takeoff Fix (https://github.com/ArduPilot/ardupilot/pull/33115) - **merged**
- EKF Bootstrap Reset (https://github.com/ArduPilot/ardupilot/pull/32202) - **merged**
- Low Noise IMU Support (https://github.com/ArduPilot/ardupilot/pull/32399) - **merged**
- getLLH Returns GPS Only When GPS Is the Position Source (https://github.com/ArduPilot/ardupilot/pull/32945) - **merged**

### Barometer & Ground Effect ###

- Baro Thrust Compensation Filter (https://github.com/ArduPilot/ardupilot/pull/32392) - **merged**
- Baro Height Datum Reset (https://github.com/ArduPilot/ardupilot/pull/32770) - **merged** (supersedes closed https://github.com/ArduPilot/ardupilot/pull/32400)
- Ground Effect Altitude/Timeout (https://github.com/ArduPilot/ardupilot/pull/32472) - **merged**

### Rates, Notch & Control ###

- Quintuple Notch (https://github.com/ArduPilot/ardupilot/pull/30994) - **in 4.7.2**
- Notch Count Cap for Quintuple Notches (https://github.com/ArduPilot/ardupilot/pull/33587) - **merged**
- NTF Log Units Fix (https://github.com/ArduPilot/ardupilot/pull/34122) - **in 4.7.2**

### Fences ###

- Fence Alt-Frame Thresholds (https://github.com/ArduPilot/ardupilot/pull/31619) - **in 4.7.2**

### Arming ###

- Arming Consistency Check Fix (https://github.com/ArduPilot/ardupilot/pull/32022) - **merged**

### VTX ###

- VTX Max Power (https://github.com/ArduPilot/ardupilot/pull/31500) - **merged**
- VTX Actual Power Reporting (https://github.com/ArduPilot/ardupilot/pull/32937) - **merged**
- MSP VTX Support (https://github.com/ArduPilot/ardupilot/pull/29768) - **merged**

### Sensors & Calibration ###

- IIS2MDC Compass Correctness Fixes and Enable by Default (https://github.com/ArduPilot/ardupilot/pull/33780) - **in 4.7.2**
- Keep the Board Rotation on the Accel during Gyro Calibration (https://github.com/ArduPilot/ardupilot/pull/33988) - **in 4.7.2**

### Boards & Boot ###

- Per-board feature enables for MambaH743v4, MatekH743 (and bdshot),
  MicoAir405v2, MicoAir743v2, MicoAir743-AIO, BETAFPV-F405, BlitzF745 (and AIO)
  and ARK_FPV - local to this branch, no PR

### Scripting & Parameters ###

- Scripting OSD (https://github.com/ArduPilot/ardupilot/pull/32045) - **merged**

## TBS_LUCID_H7 and SmallFastDronev1 Targets ##

TBS_LUCID_H7, with its V3 and OEM variants, builds as a SmallFastDrone quad: the SFD
feature enables and parameter defaults, and -O2 for the fast rate loop, with the
features an electric quad cannot use left out to make room. TBS_LUCID_H7_WING, a
plane on the same hardware, stays a standard build.

SmallFastDronev1 is the same hardware (the TBS_LUCID_H7 v2) and the same build, with
a ready-made quad setup on top: motor outputs and bidirectional DShot, the fast rate
loop, a throttle-referenced harmonic notch, the board rotation of the SFD reference
quad, and its LED, VTX and DisplayPort OSD outputs.

## The ArduPilot project is made up of: ##

- ArduCopter: [code](https://github.com/ArduPilot/ardupilot/tree/master/ArduCopter), [wiki](https://ardupilot.org/copter/index.html)

## Developer Information ##

- Github repository: <https://github.com/fossuav/smallfastdrone>

## License ##

The ArduPilot project is licensed under the GNU General Public
License, version 3.

- [Overview of license](https://ardupilot.org/dev/docs/license-gplv3.html)

- [Full Text](https://github.com/ArduPilot/ardupilot/blob/master/COPYING.txt)
