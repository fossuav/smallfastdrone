# SmallFastDrone quad build, included by the SFD boards built at -O2 for
# the fast-rate loop. -O2 makes the code about 11% larger, so what an
# electric quad cannot use, and drivers few quads carry, are left out to
# make room. sfd_quad_undo.inc undoes every line here for a board that
# includes an SFD board's hwdef without being one: keep the two in step.

# optimize for fast rates
env OPTIMIZE -O2

# SmallFastDrone features
define AP_BARO_THST_COMP_ENABLED 1
define ARMING_DELAY_MS 0
define AP_ARMING_IMU_CONSISTENCY_CHECK_TIME_MS 2000
define AP_AHRS_EKF_RESET_ENABLED 1
define AP_NOTIFY_RGBLED_RELATIVE_POSITION_ENABLED 1
define AP_OSD_EKF_LANES_ENABLED 1

# quad frames only, and no engine, fuel injection or airspeed sensor
define AP_MOTORS_FRAME_DEFAULT_ENABLED 0
define AP_MOTORS_FRAME_QUAD_ENABLED 1
define AP_ICENGINE_ENABLED 0
define HAL_EFI_ENABLED 0
define AP_AIRSPEED_ENABLED 0

# payloads, failsafes and outputs a small quad does not carry
define AP_ADVANCEDFAILSAFE_ENABLED 0
define HAL_SPRAYER_ENABLED 0
define AP_WINCH_ENABLED 0
define HAL_GENERATOR_ENABLED 0
define AP_KDECAN_ENABLED 0
define AP_PICCOLOCAN_ENABLED 0
define AP_SBUSOUTPUT_ENABLED 0
define HAL_NMEA_OUTPUT_ENABLED 0
define AP_INERTIALSENSOR_KILL_IMU_ENABLED 0
define HAL_HOTT_TELEM_ENABLED 0
define NUM_SERVO_CHANNELS 16

# radar and other uncommon range finders
define AP_RANGEFINDER_JRE_SERIAL_ENABLED 0
define AP_RANGEFINDER_AINSTEIN_LR_D1_ENABLED 0
define AP_RANGEFINDER_TOFSENSEF_I2C_ENABLED 0
define AP_RANGEFINDER_RDS02UF_ENABLED 0
define AP_RANGEFINDER_NOOPLOOP_ENABLED 0

# gimbals other than servo, MAVLink, SIYI and scripted ones
define HAL_MOUNT_XACTI_ENABLED 0
define HAL_MOUNT_TOPOTEK_ENABLED 0
define HAL_MOUNT_VIEWPRO_ENABLED 0
define HAL_MOUNT_XFROBOT_ENABLED 0
define HAL_MOUNT_ALEXMOS_ENABLED 0
define HAL_MOUNT_STORM32MAVLINK_ENABLED 0
define HAL_MOUNT_STORM32SERIAL_ENABLED 0
define HAL_MOUNT_CADDX_ENABLED 0

# GPS other than u-blox, DroneCAN, NMEA, MAVLink and MSP
define AP_GPS_SBF_ENABLED 0
define AP_GPS_SBP_ENABLED 0
define AP_GPS_SBP2_ENABLED 0
define AP_GPS_GSOF_ENABLED 0
define AP_GPS_NOVA_ENABLED 0
define AP_GPS_ERB_ENABLED 0
define AP_GPS_SIRF_ENABLED 0
