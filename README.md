# Small Fast Drone Project

The canonical ArduPilot README can be found [here](https://github.com/ArduPilot/ardupilot/blob/master/README.md)

[![Test Copter](https://github.com/ArduPilot/ardupilot/workflows/test%20copter/badge.svg?branch=master)](https://github.com/ArduPilot/ardupilot/actions/workflows/test_sitl_copter.yml)

![SFD](sfd_logo2.png)

ArduPilot is the most advanced, full-featured, and reliable open source autopilot software available.
It has been under development since 2010 by a diverse team of professional engineers, computer scientists, and community contributors.
The autopilot software is capable of controlling almost any vehicle system imaginable, from conventional airplanes, quad planes, multi-rotors, and helicopters to rovers, boats, balance bots, and even submarines. It is continually being expanded to provide support for new emerging vehicle types.

However, the need to continously support all vehicle types constrains both flash usage and feature velocity. That might be fine if you are flying a 2 ton, $100k vehicle and absolute reliability is paramount - but for smaller vehicles these constraints can be prohibitive to forward progress - and yet at the same time you also don't want to be flying the master branch the whole time. For a while I have be maintaining branches off the latest stable branch of ArduPilot that also contain features - usually features that I have developed - that are either only available in master or as PRs. These features are particularly geared to the needs to smaller, faster copters - but are also can be applicable to any size of vehicle. Maintaining these
branches has become somewhat onerous, so I have instead started this new repo giving me greater flexibility in how I managed progress. The intent is:

- To be a derivative of the latest ArduPilot beta release (This branch is for 4.7.x)
- For all included features to be open source and eventually be available in either the main ArduPilot repository or one of the fossuav repositories
- For all additional features to have been flown
- For all additional features to be documented

## Included Features ##

The branch is based on ArduPilot 4.7.0-beta1. It also includes the following PRs and features, grouped by upstream status.

### Merged upstream ###

- Fence Improvements (https://github.com/ArduPilot/ardupilot/pull/31005, https://github.com/ArduPilot/ardupilot/pull/31619)
- Recorded Origin Fix (https://github.com/ArduPilot/ardupilot/pull/32469)
- Baro Thrust Compensation Filter (https://github.com/ArduPilot/ardupilot/pull/32392)
- EK3_MAG_CAL=7 Ground and In-flight Mode (https://github.com/ArduPilot/ardupilot/pull/32200)
- EKF Zero Velocity Fusion (https://github.com/ArduPilot/ardupilot/pull/32396)
- VTX Max Power (https://github.com/ArduPilot/ardupilot/pull/31500)
- Baro Height Datum Reset (https://github.com/ArduPilot/ardupilot/pull/32770, supersedes closed https://github.com/ArduPilot/ardupilot/pull/32400)
- Arming Consistency Check Fix (https://github.com/ArduPilot/ardupilot/pull/32022)
- IMU-aided AGL Filter for Optical Flow (https://github.com/ArduPilot/ardupilot/pull/32389)
- EKF Bootstrap Reset (https://github.com/ArduPilot/ardupilot/pull/32202)
- Low Noise IMU Support (https://github.com/ArduPilot/ardupilot/pull/32399)
- Throw Mode RPM Fix (https://github.com/ArduPilot/ardupilot/pull/32955, supersedes closed https://github.com/ArduPilot/ardupilot/pull/32393)

### In review ###

- Ground Clearance Fusion Fix (https://github.com/ArduPilot/ardupilot/pull/30490)
- Motortest Error Rate (https://github.com/ArduPilot/ardupilot/pull/31274)
- Separate LEVEL Arming Check (https://github.com/ArduPilot/ardupilot/pull/32391)
- Customizable ARM_DELAY (https://github.com/ArduPilot/ardupilot/pull/32398)
- Fast Boot Parameter (https://github.com/ArduPilot/ardupilot/pull/32238)
- VALT Velocity Alt-Hold Mode (https://github.com/ArduPilot/ardupilot/pull/32270)
- Hover Z-Bias Learning (https://github.com/ArduPilot/ardupilot/pull/32471)
- Ground Effect Altitude/Timeout (https://github.com/ArduPilot/ardupilot/pull/32472)
- Baro Drift Reset on Arming (https://github.com/ArduPilot/ardupilot/pull/32768)
- Terrain Offset Reset on Ground Effect Clear (https://github.com/ArduPilot/ardupilot/pull/32553)

### Submitted, limited review so far ###

- Fast Rates (https://github.com/ArduPilot/ardupilot/pull/27893, https://github.com/ArduPilot/ardupilot/pull/29000, https://github.com/ArduPilot/ardupilot/pull/30980)
[![Fast rates](https://img.youtube.com/vi/B8Dp2jwDamU/0.jpg)](https://www.youtube.com/playlist?list=PL_O9QDs-WAVyBpf7URQQgCmNQwv_aTcMf)
- ESC Logging Control (https://github.com/ArduPilot/ardupilot/pull/30841)
- iFlight Borg H7 (https://github.com/ArduPilot/ardupilot/pull/31216)
- Pending Arm on Switch (https://github.com/ArduPilot/ardupilot/pull/32401)
- Acro Bias Inhibit (https://github.com/ArduPilot/ardupilot/pull/32473)
- AC_Loiter Brake/Drag Feed-forward Fix (https://github.com/ArduPilot/ardupilot/pull/33318)
- AGL KF Rangefinder Height Switch (https://github.com/ArduPilot/ardupilot/pull/33359)
- AGL KF Velocity velD Fusion (https://github.com/ArduPilot/ardupilot/pull/33478)
- Optical Flow Axis Lockout Recovery and Focus-Height Floor (https://github.com/ArduPilot/ardupilot/pull/33484)
- HereFlow Output Rate Correction FLOW_HF_RATEF (https://github.com/ArduPilot/ardupilot/pull/33497)
- Inhibit Z Gyro Bias from Optical Flow without Yaw Source (https://github.com/ArduPilot/ardupilot/pull/33498)
- AGL KF Accel-Z Bias Estimation (https://github.com/ArduPilot/ardupilot/pull/33507)
- Loaded Defaults Count Fix (https://github.com/ArduPilot/ardupilot/pull/33543)
- Optical Flow Relative-Aiding Fallback on GPS Loss (https://github.com/ArduPilot/ardupilot/pull/33568)
- Configurable Optical Flow Nav Gain Detune Height EK3_FLOW_GAIN_H (https://github.com/ArduPilot/ardupilot/pull/33569)

### Not yet submitted as PRs ###

- Throw Mode Improvements
- Flat-Ground Optical Flow Nav Above Rangefinder Ceiling (EK3_OPTIONS bit 6)
- Optical Flow Control-Limit and Aiding-Mode Logging - XKVL/XKF4/EKFC


## SmallFastDronev1 Target ##

There is a hardware target called SmallFastDronev1 that is designed to work optimally with this fork. The hardware itself is actually the TBS_LUCID_H7 v2, so if you get one of these flight controllers you can flash it with the target if you choose.

## The ArduPilot project is made up of: ##

- ArduCopter: [code](https://github.com/ArduPilot/ardupilot/tree/master/ArduCopter), [wiki](https://ardupilot.org/copter/index.html)

## Developer Information ##

- Github repository: <https://github.com/fossuav/smallfastdrone>

## License ##

The ArduPilot project is licensed under the GNU General Public
License, version 3.

- [Overview of license](https://ardupilot.org/dev/docs/license-gplv3.html)

- [Full Text](https://github.com/ArduPilot/ardupilot/blob/master/COPYING.txt)
