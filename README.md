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
4.7.2 release this branch is based on. Everything else is an open pull request still
under review upstream, or work local to this repo with no PR yet.

### Optical Flow & AGL Kalman Filter ###

- IMU-aided AGL Filter for Optical Flow (https://github.com/ArduPilot/ardupilot/pull/32389) - **merged**
- Ground Clearance Fusion Fix (https://github.com/ArduPilot/ardupilot/pull/32232)
- AGL KF Rangefinder Height Switch (https://github.com/ArduPilot/ardupilot/pull/33359)
- AGL KF Velocity velD Fusion (https://github.com/ArduPilot/ardupilot/pull/33478)
- Optical Flow Nav Above Rangefinder Range (https://github.com/ArduPilot/ardupilot/pull/33585)
- Optical Flow Axis Lockout Recovery (https://github.com/ArduPilot/ardupilot/pull/33484)
- Optical Flow Minimum Focus Height FLOW_HGT_MIN (https://github.com/ArduPilot/ardupilot/pull/34292)
- PX4Flow Frames Published from the Main Loop, not the I2C Thread (https://github.com/ArduPilot/ardupilot/pull/34783)
- Inhibit Z Gyro Bias from Optical Flow without Yaw Source (https://github.com/ArduPilot/ardupilot/pull/33498) - **merged**
- Keep the Covariance Consistent for States the Flow Mask Leaves Out, fixing a regression from #33498 (https://github.com/ArduPilot/ardupilot/pull/34933)
- AGL KF Velocity Floor, Decay and Coast Fixes (https://github.com/ArduPilot/ardupilot/pull/34457)
- AGL KF Accel-Z Bias Estimation (https://github.com/ArduPilot/ardupilot/pull/33507)
- Optical Flow Relative-Aiding Fallback on GPS Loss (https://github.com/ArduPilot/ardupilot/pull/33568)
- Lift the Flow Height Limit Where Flow Nav Carries On Above It (https://github.com/ArduPilot/ardupilot/pull/34380)
- Configurable Optical Flow Nav Gain Detune Height EK3_FLOW_GAIN_H (https://github.com/ArduPilot/ardupilot/pull/33569) - **merged**
- Flow Withheld by the Tilt Check No Longer Times Out Flow Aiding (https://github.com/ArduPilot/ardupilot/pull/34941)
- SRTM Height Sign Fix for Flow Scaling (https://github.com/ArduPilot/ardupilot/pull/34360) - **in 4.7.2**

### EKF & Inertial Navigation ###

- Recorded Origin Fix (https://github.com/ArduPilot/ardupilot/pull/32469) - **in 4.7.2**
- EK3_MAG_CAL=7 Ground and In-flight Mode (https://github.com/ArduPilot/ardupilot/pull/32200) - **merged**
- Yaw Anchoring while Ground-Learning in MAG_CAL=7 (https://github.com/ArduPilot/ardupilot/pull/34057) - **merged**
- EKF Zero Velocity Fusion (https://github.com/ArduPilot/ardupilot/pull/32396) - **merged**
- Zero Velocity Fusion Takeoff Fix (https://github.com/ArduPilot/ardupilot/pull/33115) - **merged**
- EKF Bootstrap Reset (https://github.com/ArduPilot/ardupilot/pull/32202) - **merged**
- Low Noise IMU Support (https://github.com/ArduPilot/ardupilot/pull/32399) - **merged**
- getLLH Returns GPS Only When GPS Is the Position Source (https://github.com/ArduPilot/ardupilot/pull/32945) - **merged**
- EKF Failsafe Message Says Whether a Variance or the Position Failed (https://github.com/ArduPilot/ardupilot/pull/32397) - **merged**
- Hover Z-Bias Learning (https://github.com/ArduPilot/ardupilot/pull/32471)
- Acro Bias Inhibit (https://github.com/ArduPilot/ardupilot/pull/32473)
- EKF Failsafe Expects a Position Only Where the Mode or Source Set Needs One (https://github.com/ArduPilot/ardupilot/pull/32514)
- EKF Failsafe Returns to Loiter or PosHold Once It Clears, FS_OPTIONS bit 6 (https://github.com/ArduPilot/ardupilot/pull/34696)
- Range Finder Height Counts as a Ground Measurement, so a Flow Landing Keeps Its Position (https://github.com/ArduPilot/ardupilot/pull/34851)
- On-Ground Height Check Fixed and Applied without a Vertical Velocity Measurement (https://github.com/ArduPilot/ardupilot/pull/34942)
- No XY Accel Bias Learning in Unaided Flight (https://github.com/ArduPilot/ardupilot/pull/34209)
- Log XKF5 and XKFA for Every Core (https://github.com/ArduPilot/ardupilot/pull/34363)
- Source Set Selection Chooses the Lane Running It (https://github.com/ArduPilot/ardupilot/pull/34456)
- OSD Status of Each EKF3 Core (https://github.com/ArduPilot/ardupilot/pull/34543)
- Flow Lockout Resets on the OSD, Optional Reset Messages (https://github.com/ArduPilot/ardupilot/pull/34630)

### Barometer & Ground Effect ###

- Baro Thrust Compensation Filter (https://github.com/ArduPilot/ardupilot/pull/32392) - **merged**
- Baro Height Datum Reset (https://github.com/ArduPilot/ardupilot/pull/32770) - **merged** (supersedes closed https://github.com/ArduPilot/ardupilot/pull/32400)
- Ground Effect Altitude/Timeout (https://github.com/ArduPilot/ardupilot/pull/32472) - **merged**
- Baro Drift Reset on Arming (https://github.com/ArduPilot/ardupilot/pull/32768)
- Reopen the Terrain Variance after Takeoff Ground Effect (https://github.com/ArduPilot/ardupilot/pull/32553)
- Protect Height Fusion from Baro Ground Effect at Takeoff (https://github.com/ArduPilot/ardupilot/pull/32972)
- Stop touchdown_expected Latching in Cruise (https://github.com/ArduPilot/ardupilot/pull/34362)
- Keep Baro Ground Effect out of a Height Source Switch (https://github.com/ArduPilot/ardupilot/pull/34432) - **merged**

### Rates, Notch & Control ###

- Quintuple Notch (https://github.com/ArduPilot/ardupilot/pull/30994) - **in 4.7.2**
- Notch Count Cap for Quintuple Notches (https://github.com/ArduPilot/ardupilot/pull/33587) - **merged**
- Fast Rates (https://github.com/ArduPilot/ardupilot/pull/27893 - **merged**, https://github.com/ArduPilot/ardupilot/pull/29000, https://github.com/ArduPilot/ardupilot/pull/30980 - **merged**)
[![Fast rates](https://img.youtube.com/vi/B8Dp2jwDamU/0.jpg)](https://www.youtube.com/playlist?list=PL_O9QDs-WAVyBpf7URQQgCmNQwv_aTcMf)
- Loiter Control Feel When the EKF Limits Ground Speed (https://github.com/ArduPilot/ardupilot/pull/33639) - **merged**
- Loiter Drag Feed-forward, LOIT_DRAG_COEF (https://github.com/ArduPilot/ardupilot/pull/33318)
- Rate Target Interpolation in the Fast Rate Thread (https://github.com/ArduPilot/ardupilot/pull/34208) - **merged**
- Throttle-Gain Boost and Throttle Mix with the Fast Rate Thread (https://github.com/ArduPilot/ardupilot/pull/34583)
- Per-Loop Rate Modifiers for the Fast Rate Thread (https://github.com/ArduPilot/ardupilot/pull/34584)
- Log Every Filtered Gyro Sample with the Rate Thread (https://github.com/ArduPilot/ardupilot/pull/34601) - **merged**
- SITL Gyro Rate Follows INS_GYRO_RATE, for testing (https://github.com/ArduPilot/ardupilot/pull/34642)
- No False Missed-Take-off Internal Error with the Fast Rate Thread (https://github.com/ArduPilot/ardupilot/pull/34678)
- INS_HNTC2 MODE and HMNCS No Longer Change across a Reboot (https://github.com/ArduPilot/ardupilot/pull/34251)
- Persist the FFT Notch Tune Tracking Mode (https://github.com/ArduPilot/ardupilot/pull/33879)
- NTF Log Units Fix (https://github.com/ArduPilot/ardupilot/pull/34122) - **in 4.7.2**

### Flight Modes ###

- VALT Velocity Alt-Hold Mode, and FS_EKF_ACTION 4 to fail over to it (https://github.com/ArduPilot/ardupilot/pull/32270)
- Advanced Land Failsafe, LAND_FS_OPTIONS (https://github.com/ArduPilot/ardupilot/pull/34210)
- SmartRTL Return Point Reset in Flight from Lua, with a Switch Applet (https://github.com/ArduPilot/ardupilot/pull/35119)
- Throw Mode Improvements (https://github.com/ArduPilot/ardupilot/pull/32475): drop detection and
  recovery, quaternion uprighting, operation without GPS, next-mode selection,
  EKF source-set switching on completion, stage feedback on the OSD and to the
  GCS, and mid-stick arming with the motors stopped

### Fences ###

- Fence Alt-Frame Thresholds (https://github.com/ArduPilot/ardupilot/pull/31619) - **in 4.7.2**
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

### LEDs ###

- RGB LEDs Show a Non-GPS Position, on for the SFD Boards (https://github.com/ArduPilot/ardupilot/pull/35122)

### ESC, Motors & Logging ###

- ESC Logging Control (https://github.com/ArduPilot/ardupilot/pull/30841)
- Motortest Error Rate (https://github.com/ArduPilot/ardupilot/pull/31274)
- Reject Invalid GCR Quintets in DShot Telemetry (https://github.com/ArduPilot/ardupilot/pull/33990) - **merged**
- True CPU Load from the Idle Threads, Logged in PM2 and Checked before Take-off, TKOFF_CPU_AVG/PEAK (https://github.com/ArduPilot/ardupilot/pull/33366)

### Sensors & Calibration ###

- IIS2MDC Compass Correctness Fixes and Enable by Default (https://github.com/ArduPilot/ardupilot/pull/33780) - **in 4.7.2**
- Keep the Board Rotation on the Accel during Gyro Calibration (https://github.com/ArduPilot/ardupilot/pull/33988) - **in 4.7.2**
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
- A Script's OSD Writes Go Out in the Same Frame as the Screen (https://github.com/ArduPilot/ardupilot/pull/34809) - **merged**
- MSP DisplayPort Requests Answered from the OSD Thread (https://github.com/ArduPilot/ardupilot/pull/34882) - **merged**
- Loaded Defaults Count Fix (https://github.com/ArduPilot/ardupilot/pull/33543)

## Upstreaming Order ##

The list above groups features by function. This one sequences the **open** PRs in
the order that makes them easiest to merge: dependencies before dependents, small
and already-approved work first, and the heavy diffs last so they rebase onto a
smaller backlog.

Annotations are the state on 2026-11-11, not a promise - re-check before acting on
them. "AI" is the automated review's verdict at the PR's current head, with its
date; "stale" means the PR has been pushed since that round. "Conflicts" means
GitHub reports a merge conflict with master. Regenerate the AI column with
`Tools/SFD/ai_review_status.py`.

### 1. Approved, waiting only on a merge

1. #34457 AP_NavEKF3: AGL KF velocity decay and floor wind-up - 8 files, no human review - AI: accept (11-07). Merge first: #33507 is stacked on it.
2. #33507 AP_NavEKF3: estimate accel-Z bias in the AGL KF - 11 files, approved by rishabsingh3003 - AI: comment (11-07). Carries #34457's twelve commits.
3. #34251 Copter, Plane: stop INS_HNTC2 MODE and HMNCS changing across a reboot - 2 files, approved by IamPete1, now against ArduPilot-4.7 since master dropped the conversion - AI: comment (10-30)
4. #32391 Copter: add separate LEVEL arming check for lean angle - 3 files, approved by peterbarker - AI: request changes (09-30)
5. #29000 Copter: switch off fast rate while doing temperature calibration - 2 files, approved by tridge - AI: accept (10-31)

#33879 pairs with #34251: the latter stops seeding the INS_HNTC2 defaults, the former forces the tuned values into storage so they survive a default change. No human has reviewed it.

6. #33879 FFT notch tune: persist the tracking mode and name the notch used - 4 files, no human review - AI: accept (11-02)

### 2. Small self-contained fixes

1. #34678 Copter: avoid a false missed-take-off internal error with the fast rate thread - 1 file - AI: accept (11-11)
2. #33543 AP_Param: publish actual loaded defaults count, not the pre-count - 1 file - AI: accept (11-02)
3. #34642 AP_InertialSensor: let the SITL gyro rate follow INS_GYRO_RATE - 3 files, SITL only - AI: accept (11-02)
4. #32514 Copter: expect a position in the EKF failsafe only where the mode or source set needs one - 6 files, changes requested by peterbarker (10-06) - AI: accept (11-03)
5. #33318 AC_WPNav: feed the vehicle's own drag forward in Loiter (LOIT_DRAG_COEF) - 5 files - AI: accept (11-07)
6. #31005 Copter: don't fall out of the sky at zero throttle on min alt fences - 3 files - AI: accept (11-07)
7. #34363 AP_NavEKF3: log XKF5 and XKFA for every core - 4 files - AI: accept (10-11)
8. #30841 Control ESC Logging - 3 files - AI: comment (11-07)
9. #32398 Copter: take the arming delay in milliseconds and compile it out at zero - 6 files, changes requested by peterbarker - AI: comment (09-30)
10. #32238 Add FAST_BOOT bitmask parameter - 5 files - AI: comment (10-30)
11. #31274 Motortest error rate - 22 files - **conflicts**, largely superseded upstream - AI: comment (11-07)
12. #32232 AP_NavEKF3: ground clearance fusion fix - 7 files, changes requested by tridge - AI: comment (10-30)
13. #34696 Copter: return to Loiter or PosHold when the EKF failsafe clears - 6 files, self-reviewed in three rounds - no AI review yet
14. #35122 AP_Notify: show a non-GPS position on RGB LEDs - 5 files, off by default - no AI review yet
15. #35119 AP_SmartRTL: add an in-flight return point reset for Lua scripts - 10 files - AI: comment (11-11)

### 3. Baro and ground effect

#32972 is stacked on #32768, so that pair merges in order. #32768 is under maintainer review; leave it unchanged until that finishes.

1. #32768 AP_NavEKF3: clear baro temperature drift on arming - 26 files, changes requested by peterbarker and tridge - AI: comment (11-04)
2. #32972 AP_NavEKF3: protect height fusion from baro ground effect at takeoff - 26 files - AI: comment (11-05)
3. #32553 AP_NavEKF3: reopen the terrain variance after takeoff ground effect - 5 files, changes requested by rishabsingh3003 - AI: accept (10-11)
4. #34362 AP_GroundEffect: stop touchdown_expected latching in cruise - 3 files - AI: accept (10-04)

### 4. Optical flow and the AGL Kalman filter

#34933 fixes a master regression from #33498 and #34783 a PX4Flow threading race; neither depends on the rest. #33359 introduces the AGL-KF height the rest build on. #34380 is stacked on #33585, and #33568 must follow #34380: once #33568 turns the flow height limit on, master's AC_Avoid turns a climb above it into a descent. #34851 changes what the ground offset's validity means, which #33585 and #34380 also read, so whichever merges second re-checks them. #34630 is stacked on #33484 and #34543.

1. #34933 AP_NavEKF3: keep the covariance consistent for states flow leaves out - 1 file, fixes #34932 - AI: accept (10-29)
2. #34783 AP_OpticalFlow: publish PX4Flow frames from the main loop - 2 files - AI: accept (10-18)
3. #33359 AP_NavEKF3: use the AGL KF for the optical-flow rangefinder height switch and observation - 7 files - AI: comment (11-04)
4. #33478 AP_NavEKF3: fuse AGL KF velocity as a velD observation, and fix its decay - 9 files - AI: comment (11-07)
5. #33585 AP_NavEKF3: keep optical flow nav alive above the rangefinder range - 14 files, changes requested by peterbarker (11-11) - AI: accept (11-05)
6. #34380 AP_NavEKF3: lift the flow height limit where flow nav carries on above it - 14 files - AI: request changes (11-05)
7. #33568 AP_NavEKF3: fall back to relative aiding when optical flow replaces lost GPS - 4 files - AI: request changes (11-04), on merge order only; rmackay9 linked a 4.7.1 user report it fixes
8. #34851 AP_NavEKF3: count range finder height as a ground measurement - 5 files - AI: comment (11-03)
9. #33484 AP_NavEKF3: recover horizontal velocity from single-axis optical flow lockout - 21 files - AI: comment (11-11)
10. #34292 AP_OpticalFlow: add FLOW_HGT_MIN, the sensor's minimum focus height - 30 files, changes requested by peterbarker and tridge - AI: accept (10-31); split out of #33484, so it follows it
11. #34941 AP_NavEKF3: do not time out flow aiding while the tilt check withholds it - 5 files, changes requested by tridge - AI: comment (11-10)
12. #34456 AP_NavEKF3: select the lane that runs the source set being asked for - 14 files, changes requested by tridge - AI: comment (10-30)
13. #34543 AP_OSD: show the status of each EKF3 core - 24 files - AI: comment (11-02)
14. #34630 AP_NavEKF3: show flow resets and the quality floor on the OSD, make reset messages optional - 38 files - AI: comment (11-11)
15. #34942 AP_NavEKF3: fix the on-ground height check and apply it without a vertical velocity - 9 files - AI: comment (11-07)

### 5. Accel bias

#32473 overlaps #32471 across 30 files, so merging them the other way round means redoing the conflict.

1. #32471 AP_NavEKF3: hover Z-bias learning for vibration rectification - 38 files, approved by priseborough - AI: accept (11-07)
2. #32473 Copter: add ACC_ZBIAS_LEARN bit 3 to inhibit accel bias learning in acro - 39 files - AI: accept (11-07)
3. #34209 AP_NavEKF3: do not learn XY accel bias in unaided flight - 3 files, approved by rishabsingh3003, changes requested by peterbarker and tridge - AI: accept (10-30)

### 6. Fast rates

#27893, #30980 and #34208 have merged upstream; #29000 is in group 1.

1. #34583 AC_AttitudeControl: fix throttle-gain boost and throttle mix with the fast rate thread - 4 files - AI: accept (11-08)
2. #34584 AC_AttitudeControl: record per-loop rate modifiers for the fast rate thread - 5 files, stacked on #34583 - AI: comment (11-08)

### 7. Larger features and boards

Bigger diffs and new parameters; these want the small fixes out of the way first.

1. #32401 Copter: add pending arm on switch for in-air arming - 12 files, changes requested by peterbarker - AI: accept (11-11)
2. #34210 Copter: add advanced land failsafe (LAND_FS_OPTIONS bit 0) - 10 files - AI: accept (10-31)
3. #32475 Copter: throw mode improvements - 7 files - AI: accept (11-11)
4. #32270 Copter: add VALT velocity alt-hold flight mode - 18 files - AI: request changes (10-13)
5. #31216 AP_HAL_ChibiOS: iFlight Borg H7 - 8 files, changes requested by Hwurzburg - AI: accept (11-02)
6. #31770 AP_Bootloader: add DFU mode via STM32 system bootloader - 5 files, changes requested by tpwrules - AI: request changes, stale since 09-12

#33366 (robertlong13's true CPU load) is carried here but is not ours to sequence.


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
