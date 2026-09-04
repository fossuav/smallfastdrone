# Small Fast Drone Project

The canonical ArduPilot README can be found [here](https://github.com/ArduPilot/ardupilot/blob/master/README.md)

[![Test Copter](https://github.com/ArduPilot/ardupilot/workflows/test%20copter/badge.svg?branch=master)](https://github.com/ArduPilot/ardupilot/actions/workflows/test_sitl_copter.yml)

![SFD](sfd_logo2.png)

ArduPilot is the most advanced, full-featured, and reliable open source autopilot software available.
It has been under development since 2010 by a diverse team of professional engineers, computer scientists, and community contributors.
The autopilot software is capable of controlling almost any vehicle system imaginable, from conventional airplanes, quad planes, multi-rotors, and helicopters to rovers, boats, balance bots, and even submarines. It is continually being expanded to provide support for new emerging vehicle types.

However, the need to continously support all vehicle types constrains both flash usage and feature velocity. That might be fine if you are flying a 2 ton, $100k vehicle and absolute reliability is paramount - but for smaller vehicles these constraints can be prohibitive to forward progress - and yet at the same time you also don't want to be flying the master branch the whole time. For a while I have be maintaining branches off the latest stable branch of ArduPilot that also contain features - usually features that I have developed - that are either only available in master or as PRs. These features are particularly geared to the needs to smaller, faster copters - but are also can be applicable to any size of vehicle. Maintaining these
branches has become somewhat onerous, so I have instead started this new repo giving me greater flexibility in how I managed progress. The intent is:

- To be a derivative of the latest ArduPilot release (this branch tracks 4.7.x, currently 4.7.1)
- For all included features to be open source and eventually be available in either the main ArduPilot repository or one of the fossuav repositories
- For all additional features to have been flown
- For all additional features to be documented

## Included Features ##

The branch is based on ArduPilot 4.7.1. It also includes the features below,
grouped by function.

Entries marked **merged** have landed in ArduPilot master since they were picked
up here, so they will arrive in a future stable release; they are carried on this
branch because 4.7 predates them. Everything else is an open pull request still
under review upstream, or work local to this repo with no PR yet.

### Optical Flow & AGL Kalman Filter ###

- IMU-aided AGL Filter for Optical Flow (https://github.com/ArduPilot/ardupilot/pull/32389) - **merged**
- Ground Clearance Fusion Fix (https://github.com/ArduPilot/ardupilot/pull/32232)
- AGL KF Rangefinder Height Switch (https://github.com/ArduPilot/ardupilot/pull/33359)
- AGL KF Velocity velD Fusion (https://github.com/ArduPilot/ardupilot/pull/33478)
- Optical Flow Nav Above Rangefinder Range (https://github.com/ArduPilot/ardupilot/pull/33585)
- Optical Flow Axis Lockout Recovery (https://github.com/ArduPilot/ardupilot/pull/33484)
- Optical Flow Minimum Focus Height FLOW_HGT_MIN (https://github.com/ArduPilot/ardupilot/pull/34292)
- HereFlow Output Rate Correction FLOW_HF_RATEF (https://github.com/ArduPilot/ardupilot/pull/33497)
- Inhibit Z Gyro Bias from Optical Flow without Yaw Source (https://github.com/ArduPilot/ardupilot/pull/33498)
- AGL KF Accel-Z Bias Estimation (https://github.com/ArduPilot/ardupilot/pull/33507)
- Optical Flow Relative-Aiding Fallback on GPS Loss (https://github.com/ArduPilot/ardupilot/pull/33568)
- Configurable Optical Flow Nav Gain Detune Height EK3_FLOW_GAIN_H (https://github.com/ArduPilot/ardupilot/pull/33569)

### EKF & Inertial Navigation ###

- Recorded Origin Fix (https://github.com/ArduPilot/ardupilot/pull/32469) - **merged**
- EK3_MAG_CAL=7 Ground and In-flight Mode (https://github.com/ArduPilot/ardupilot/pull/32200) - **merged**
- Yaw Anchoring while Ground-Learning in MAG_CAL=7 (https://github.com/ArduPilot/ardupilot/pull/34057) - **merged**
- EKF Zero Velocity Fusion (https://github.com/ArduPilot/ardupilot/pull/32396) - **merged**
- Zero Velocity Fusion Takeoff Fix (https://github.com/ArduPilot/ardupilot/pull/33115) - **merged**
- EKF Bootstrap Reset (https://github.com/ArduPilot/ardupilot/pull/32202) - **merged**
- Low Noise IMU Support (https://github.com/ArduPilot/ardupilot/pull/32399) - **merged**
- getLLH Returns GPS Only When GPS Is the Position Source (https://github.com/ArduPilot/ardupilot/pull/32945) - **merged**
- Hover Z-Bias Learning (https://github.com/ArduPilot/ardupilot/pull/32471)
- Acro Bias Inhibit (https://github.com/ArduPilot/ardupilot/pull/32473)
- EKF Failsafe Gate Reset on Source Set Change (https://github.com/ArduPilot/ardupilot/pull/32514)
- No XY Accel Bias Learning in Unaided Flight (https://github.com/ArduPilot/ardupilot/pull/34209)

### Barometer & Ground Effect ###

- Baro Thrust Compensation Filter (https://github.com/ArduPilot/ardupilot/pull/32392) - **merged**
- Baro Height Datum Reset (https://github.com/ArduPilot/ardupilot/pull/32770) - **merged** (supersedes closed https://github.com/ArduPilot/ardupilot/pull/32400)
- Ground Effect Altitude/Timeout (https://github.com/ArduPilot/ardupilot/pull/32472) - **merged**
- Baro Drift Reset on Arming (https://github.com/ArduPilot/ardupilot/pull/32768)
- Terrain Offset Reset on Ground Effect Clear (https://github.com/ArduPilot/ardupilot/pull/32553)
- Protect Height Fusion from Baro Ground Effect at Takeoff (https://github.com/ArduPilot/ardupilot/pull/32972)

### Rates, Notch & Control ###

- Quintuple Notch (https://github.com/ArduPilot/ardupilot/pull/30994) - **merged**
- Notch Count Cap for Quintuple Notches (https://github.com/ArduPilot/ardupilot/pull/33587) - **merged**
- Fast Rates (https://github.com/ArduPilot/ardupilot/pull/27893, https://github.com/ArduPilot/ardupilot/pull/29000, https://github.com/ArduPilot/ardupilot/pull/30980)
[![Fast rates](https://img.youtube.com/vi/B8Dp2jwDamU/0.jpg)](https://www.youtube.com/playlist?list=PL_O9QDs-WAVyBpf7URQQgCmNQwv_aTcMf)
- AC_Loiter Brake/Drag Feed-forward Fix (https://github.com/ArduPilot/ardupilot/pull/33318)
- Rate Target Interpolation in the Fast Rate Thread (https://github.com/ArduPilot/ardupilot/pull/34208)
- Persist the Fixed Notch Conversion to INS_HNTC2 (https://github.com/ArduPilot/ardupilot/pull/34251)
- NTF Log Units Fix (https://github.com/ArduPilot/ardupilot/pull/34122) - **merged**

### Flight Modes ###

- VALT Velocity Alt-Hold Mode (https://github.com/ArduPilot/ardupilot/pull/32270)
- Advanced Land Failsafe, LAND_FS_OPTIONS (https://github.com/ArduPilot/ardupilot/pull/34210)
- Throw mode improvements - local to this branch, no PR yet: drop detection and
  recovery, quaternion uprighting, operation without GPS, next-mode selection,
  EKF source-set switching on completion, stage feedback on the OSD and to the
  GCS, and mid-stick arming with the motors stopped

### Fences ###

- Fence Alt-Frame Thresholds (https://github.com/ArduPilot/ardupilot/pull/31619) - **merged**
- Fence Min-Alt Disarm (https://github.com/ArduPilot/ardupilot/pull/31005)

### Arming ###

- Arming Consistency Check Fix (https://github.com/ArduPilot/ardupilot/pull/32022) - **merged**
- Separate LEVEL Arming Check (https://github.com/ArduPilot/ardupilot/pull/32391)
- Customizable ARM_DELAY (https://github.com/ArduPilot/ardupilot/pull/32398)
- Pending Arm on Switch (https://github.com/ArduPilot/ardupilot/pull/32401)

### VTX ###

- VTX Max Power (https://github.com/ArduPilot/ardupilot/pull/31500) - **merged**
- VTX Actual Power Reporting (https://github.com/ArduPilot/ardupilot/pull/32937) - **merged**
- MSP VTX Support (https://github.com/ArduPilot/ardupilot/pull/29768) - **merged**

### ESC, Motors & Logging ###

- ESC Logging Control (https://github.com/ArduPilot/ardupilot/pull/30841)
- Motortest Error Rate (https://github.com/ArduPilot/ardupilot/pull/31274)
- Reject Invalid GCR Quintets in DShot Telemetry (https://github.com/ArduPilot/ardupilot/pull/33990) - **merged**

### Sensors & Calibration ###

- IIS2MDC Compass Correctness Fixes and Enable by Default (https://github.com/ArduPilot/ardupilot/pull/33780) - **merged**
- Keep the Board Rotation on the Accel during Gyro Calibration (https://github.com/ArduPilot/ardupilot/pull/33988) - **merged**
- ICP201XX Lower Noise Mode and FIR Settling Fix (https://github.com/ArduPilot/ardupilot/pull/34120) - **merged**

### Boards & Boot ###

- iFlight Borg H7 (https://github.com/ArduPilot/ardupilot/pull/31216)
- DFU Mode via System Bootloader (https://github.com/ArduPilot/ardupilot/pull/31770)
- Fast Boot Parameter (https://github.com/ArduPilot/ardupilot/pull/32238)
- Per-board feature enables for MambaH743v4, MatekH743 (and bdshot),
  MicoAir405v2, MicoAir743v2, MicoAir743-AIO, BETAFPV-F405, BlitzF745 (and AIO)
  and ARK_FPV - local to this branch, no PR

### Scripting & Parameters ###

- Scripting OSD (https://github.com/ArduPilot/ardupilot/pull/32045) - **merged**
- Loaded Defaults Count Fix (https://github.com/ArduPilot/ardupilot/pull/33543)

## Upstreaming Order ##

The list above groups features by function. This one sequences the **open** PRs in
the order that makes them easiest to merge: dependencies before dependents, small
and already-approved work first, and the heavy diffs last so they rebase onto a
smaller backlog.

Annotations are the state at the last review sweep, not a promise - re-check before
acting on them.

### 1. Approved, waiting only on a merge

1. #34251 Copter, Plane: persist the fixed notch conversion to INS_HNTC2 - 2 files, reviewed by IamPete1 - CI failing
2. #32391 Copter: add separate LEVEL arming check for lean angle - 3 files, reviewed by IamPete1 and others - CI failing
3. #29000 Copter: switch off fast rate while doing temperature calibration - 1 file, reviewed by peterbarker and others - **needs rebase**

### 2. Small self-contained fixes

1. #33543 AP_Param: publish actual loaded defaults count, not the pre-count - 1 file, comments only, no review
2. #33318 AC_Loiter: remove drag from feed-forward accel to fix loiter overshoot - 2 files, reviewed by lthall and others
3. #31005 Copter: don't fall out of the sky at zero throttle on min alt fences - 2 files, reviewed by IamPete1 and others
4. #33497 AP_OpticalFlow: add FLOW_HF_RATEF to correct HereFlow output rate - 4 files, reviewed by dakejahl and others
5. #30841 Control ESC Logging - 5 files, reviewed by peterbarker
6. #32398 Copter: make ARM_DELAY customizable via hwdef and avoid race in ARMING_DELAY_MSEC - 6 files, reviewed by peterbarker and others
7. #32238 Add FAST_BOOT bitmask parameter - 4 files, reviewed by IamPete1 and others - **needs rebase**
8. #31274 Motortest error rate - 6 files, reviewed by IamPete1 and others - **needs rebase**
9. #32232 AP_NavEKF3: ground clearance fusion fix - 4 files, reviewed by IamPete1 and others

### 3. Baro and ground effect

#32972 is stacked on #32768, so that pair merges in order.

1. #32768 AP_NavEKF3: clear baro temperature drift on arming - 20 files, reviewed by peterbarker and others, AI-reviewed
2. #32972 AP_NavEKF3: protect height fusion from baro ground effect at takeoff - 21 files, comments only, no review, AI-reviewed
3. #32553 AP_NavEKF3: reset terrain offset from baro when ground effect clears - 3 files, reviewed by rishabsingh3003 and others
4. #32514 Copter: reset EKF failsafe gate on source set change - 2 files, comments only, no review

### 4. Optical flow and the AGL Kalman filter

#33359 introduces the AGL-KF height the rest build on, and #33585 is stacked on #33478. #33569 currently detunes against the raw terrain state and should adopt #33359's height once that lands.

1. #33359 AP_NavEKF3: use the AGL KF for the optical-flow rangefinder height switch and observation - 1 file, comments only, no review, AI-reviewed
2. #33478 AP_NavEKF3: fuse rangefinder-aided AGL KF velocity as a velD observation - 8 files, **no review at all**
3. #33585 AP_NavEKF3: keep optical flow nav alive above the rangefinder range - 9 files, **no review at all**
4. #33507 AP_NavEKF3: estimate accel-Z bias in the AGL KF - 8 files, comments only, no review
5. #33484 AP_NavEKF3: recover horizontal velocity from single-axis optical flow lockout - 12 files, **no review at all** - **needs rebase**
6. #33568 AP_NavEKF3: fall back to relative aiding when optical flow replaces lost GPS - 4 files, **no review at all**
7. #33569 AP_NavEKF3: make the optical-flow nav gain detune height configurable (EK3_FLOW_GAIN_H) - 3 files, reviewed by peterbarker
8. #33498 AP_NavEKF3: inhibit Z gyro bias from optical flow with no yaw reference - 4 files, comments only, no review, AI-reviewed

### 5. Accel bias

#32473 overlaps #32471 across 30 files, so merging them the other way round means redoing the conflict.

1. #32471 AP_NavEKF3: hover Z-bias learning for vibration rectification - 31 files, reviewed by priseborough - **needs rebase**
2. #32473 Copter: inhibit accel bias learning during acro flight - 32 files, comments only, no review - **needs rebase**
3. #34209 AP_NavEKF3: do not learn XY accel bias in unaided flight - 2 files, reviewed by peterbarker and others, AI-reviewed

### 6. Fast rates

#27893 is the foundation the other two build on.

1. #27893 AP_InertialSensor: support fast rate primary on MPU6000 - 2 files, comments only, no review, AI-reviewed
2. #30980 Copter: fix compassmot so that it works with the rate thread - 3 files, comments only, no review
3. #34208 Copter: interpolate the rate target in the fast rate thread - 11 files, reviewed by peterbarker, AI-reviewed

### 7. Larger features and boards

Bigger diffs and new parameters; these want the small fixes out of the way first.

1. #32401 Copter: add pending arm on switch for in-air arming - 7 files, reviewed by IamPete1 and others
2. #34210 Copter: add advanced land failsafe (LAND_FS_OPTIONS bit 0) - 10 files, comments only, no review - **needs rebase**
3. #32475 Copter: throw mode improvements - 8 files, reviewed by lthall
4. #32270 Copter: add VALT velocity alt-hold flight mode - 13 files, reviewed by lthall and others - **needs rebase**
5. #31216 AP_HAL_ChibiOS: iFlight Borg H7 - 6 files, reviewed by Hwurzburg
6. #31770 AP_Bootloader: add DFU mode via STM32 system bootloader - 5 files, reviewed by peterbarker and others


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
