#include "Copter.h"

#if MODE_THROW_ENABLED

// throw_init - initialise throw controller
bool ModeThrow::init(bool ignore_checks)
{
#if FRAME_CONFIG == HELI_FRAME
    // do not allow helis to use throw to start
    return false;
#endif

    // do not enter the mode when already armed or when flying
    if (motors->armed()) {
        return false;
    }

    // init state
    stage = Throw_Disarmed;
    nextmode_attempted = false;
    xy_controller_active = false;
    drop_confirm_start_ms = 0;
    last_stage_msg_ms = 0;

    // initialise pos controller speed and acceleration
    pos_control->set_max_speed_accel_xy(wp_nav->get_default_speed_xy(), BRAKE_MODE_DECEL_RATE);
    pos_control->set_correction_speed_accel_xy(wp_nav->get_default_speed_xy(), BRAKE_MODE_DECEL_RATE);

    // set vertical speed and acceleration limits
    if (g2.throw_type == ThrowType::Drop) {
        pos_control->set_max_speed_accel_z(THROW_DROP_SPEED_Z, THROW_DROP_SPEED_Z, THROW_DROP_DECEL_RATE);
        pos_control->set_correction_speed_accel_z(THROW_DROP_SPEED_Z, THROW_DROP_SPEED_Z, THROW_DROP_DECEL_RATE);
    } else {
        pos_control->set_max_speed_accel_z(BRAKE_MODE_SPEED_Z, BRAKE_MODE_SPEED_Z, BRAKE_MODE_DECEL_RATE);
        pos_control->set_correction_speed_accel_z(BRAKE_MODE_SPEED_Z, BRAKE_MODE_SPEED_Z, BRAKE_MODE_DECEL_RATE);
    }

    return true;
}

// runs the throw to start controller
// should be called at 100hz or more
void ModeThrow::run()
{
    /* Throw State Machine
    Throw_Disarmed - motors are off
    Throw_Detecting -  motors are on and we are waiting for the throw
    Throw_Uprighting - the throw has been detected and the copter is being uprighted
    Throw_HgtStabilise - the copter is kept level and  height is stabilised about the target height
    Throw_PosHold - the copter is kept at a constant position and height
    */

    if (!motors->armed()) {
        // state machine entry is always from a disarmed state
        stage = Throw_Disarmed;

    } else if (stage == Throw_Disarmed && motors->armed()) {
        stage = Throw_Detecting;

    } else if (stage == Throw_Detecting && throw_detected()){
        copter.set_land_complete(false);
        stage = Throw_Wait_Throttle_Unlimited;

        // Cancel the waiting for throw tone sequence
        AP_Notify::flags.waiting_for_throw = false;

    } else if (stage == Throw_Wait_Throttle_Unlimited &&
               motors->get_spool_state() == AP_Motors::SpoolState::THROTTLE_UNLIMITED) {
        stage = Throw_Uprighting;
    } else if (stage == Throw_Uprighting && throw_attitude_good()) {
        stage = Throw_HgtStabilise;
        hgt_stabilise_start_ms = AP_HAL::millis();

        // initialise the z controller
        pos_control->init_z_controller_no_descent();

        // initialise the demanded height below/above the throw height from user parameters
        // this allows for rapidly clearing surrounding obstacles
        if (g2.throw_type == ThrowType::Drop) {
            pos_control->set_pos_desired_z_cm(inertial_nav.get_position_z_up_cm() - g.throw_altitude_descend * 100.0f);
        } else {
            pos_control->set_pos_desired_z_cm(inertial_nav.get_position_z_up_cm() + g.throw_altitude_ascend * 100.0f);
        }

        // Set the auto_arm status to true to avoid a possible automatic disarm caused by selection of an auto mode with throttle at minimum
        copter.set_auto_armed(true);

    } else if (stage == Throw_HgtStabilise && throw_height_good() &&
               (throw_velocity_good() || (AP_HAL::millis() - hgt_stabilise_start_ms > 2000))) {
        // check if we have horizontal position for PosHold
        nav_filter_status filt_status = inertial_nav.get_filter_status();
        if (filt_status.flags.horiz_pos_abs) {
            gcs().send_text(MAV_SEVERITY_INFO,"Throw height achieved, good position");
            stage = Throw_PosHold;

            // initialise position controller
            pos_control->init_xy_controller();
            xy_controller_active = true;
        } else {
            gcs().send_text(MAV_SEVERITY_INFO,"Throw height achieved, lost position");
            stage = Throw_PosHold;
        }

        // Set the auto_arm status to true to avoid a possible automatic disarm caused by selection of an auto mode with throttle at minimum
        copter.set_auto_armed(true);
    } else if (stage == Throw_PosHold && (!xy_controller_active || throw_position_good())) {
        if (!nextmode_attempted) {
            // Warn if throttle is low — in ALT_HOLD, below mid-stick commands descent
            if (channel_throttle->get_control_in() < copter.get_throttle_mid() - copter.g.throttle_deadzone) {
                gcs().send_text(MAV_SEVERITY_WARNING, "Throttle low - losing altitude");
            }
            // switch EKF source set if configured
            const int8_t srcset = g2.throw_srcset.get();
            if (srcset >= 1 && srcset <= 3) {
                AP::ahrs().set_posvelyaw_source_set(AP_NavEKF_Source::SourceSetSelection(srcset - 1));
                gcs().send_text(MAV_SEVERITY_INFO, "EKF Source Set %d", srcset);
            }

            switch ((Mode::Number)g2.throw_nextmode.get()) {
                case Mode::Number::AUTO:
                case Mode::Number::GUIDED:
                case Mode::Number::RTL:
                case Mode::Number::LAND:
                case Mode::Number::BRAKE:
                case Mode::Number::LOITER:
                case Mode::Number::STABILIZE:
                case Mode::Number::ALT_HOLD:
                    set_mode((Mode::Number)g2.throw_nextmode.get(), ModeReason::THROW_COMPLETE);
                    break;
                default:
                    // do nothing
                    break;
            }
            nextmode_attempted = true;
        }
    }

    // Throw State Processing
    switch (stage) {

    case Throw_Disarmed:

        // prevent motors from rotating before the throw is detected unless enabled by the user
        if (g.throw_motor_start == PreThrowMotorState::RUNNING) {
            motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::GROUND_IDLE);
        } else {
            motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::SHUT_DOWN);
        }

        // demand zero throttle (motors will be stopped anyway) and continually reset the attitude controller
        attitude_control->reset_yaw_target_and_rate();
        attitude_control->reset_rate_controller_I_terms();
        attitude_control->set_throttle_out(0,true,g.throttle_filt);
        break;

    case Throw_Detecting:

        // prevent motors from rotating before the throw is detected unless enabled by the user
        if (g.throw_motor_start == PreThrowMotorState::RUNNING) {
            motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::GROUND_IDLE);
        } else {
            motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::SHUT_DOWN);
        }

        // Hold throttle at zero during the throw and continually reset the attitude controller
        attitude_control->reset_yaw_target_and_rate();
        attitude_control->reset_rate_controller_I_terms();
        attitude_control->set_throttle_out(0,true,g.throttle_filt);

        // Play the waiting for throw tone sequence to alert the user
        AP_Notify::flags.waiting_for_throw = true;

        break;

    case Throw_Wait_Throttle_Unlimited:

        // set motors to full range
        motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::THROTTLE_UNLIMITED);

        break;

    case Throw_Uprighting:

        // set motors to full range
        motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::THROTTLE_UNLIMITED);

        // demand a level roll/pitch attitude with zero yaw rate
        attitude_control->input_euler_angle_roll_pitch_euler_rate_yaw(0.0f, 0.0f, 0.0f);

        // For drops use hover throttle with angle boost.  When commanding level
        // the boost factor is 1.0, so throttle output equals hover throttle
        // once upright — giving a smooth 1g transition with no overshoot.
        // For upward throws use 50% without boost to maximise righting moment.
        if (g2.throw_type == ThrowType::Drop) {
            attitude_control->set_throttle_out(motors->get_throttle_hover(), true, g.throttle_filt);
        } else {
            attitude_control->set_throttle_out(0.5f, false, g.throttle_filt);
        }

        break;

    case Throw_HgtStabilise:

        // set motors to full range
        motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::THROTTLE_UNLIMITED);

        // call attitude controller
        attitude_control->input_euler_angle_roll_pitch_euler_rate_yaw(0.0f, 0.0f, 0.0f);

        // call height controller
        pos_control->set_pos_target_z_from_climb_rate_cm(0.0f);
        pos_control->update_z_controller();

        break;

    case Throw_PosHold:

        // set motors to full range
        motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::THROTTLE_UNLIMITED);

        if (xy_controller_active) {
            // use position controller to stop
            Vector2f vel;
            Vector2f accel;
            pos_control->input_vel_accel_xy(vel, accel);
            pos_control->update_xy_controller();

            // call attitude controller
            attitude_control->input_thrust_vector_rate_heading(pos_control->get_thrust_vector(), 0.0f);
        } else {
            // no horizontal position available, hold level attitude only
            attitude_control->input_euler_angle_roll_pitch_euler_rate_yaw(0.0f, 0.0f, 0.0f);
        }

        // call height controller
        pos_control->set_pos_target_z_from_climb_rate_cm(0.0f);
        pos_control->update_z_controller();

        break;
    }

    // update OSD mode string and send periodic GCS stage messages at 2Hz
    {
        const uint32_t now_ms = AP_HAL::millis();
        const char *mode_str = "THRW";
        const char *stage_msg = nullptr;
        switch (stage) {
        case Throw_Disarmed:
            break;
        case Throw_Detecting:
            // flash mode string while armed and waiting for throw
            mode_str = ((now_ms / 500) % 2 == 0) ? "THRW" : "    ";
            stage_msg = "Waiting for throw";
            break;
        case Throw_Wait_Throttle_Unlimited:
            mode_str = ((now_ms / 250) % 2 == 0) ? "THR!" : "    ";
            stage_msg = "Throw detected";
            break;
        case Throw_Uprighting:
            mode_str = ((now_ms / 250) % 2 == 0) ? "THR!" : "    ";
            stage_msg = "Throw detected";
            break;
        case Throw_HgtStabilise:
            mode_str = "THHT";
            stage_msg = "Stabilizing throw height";
            break;
        case Throw_PosHold:
            mode_str = "THPH";
            stage_msg = "Throw holding position";
            break;
        }
        AP::notify().set_flight_mode_str(mode_str);

        if (stage_msg != nullptr && (now_ms - last_stage_msg_ms >= 500)) {
            last_stage_msg_ms = now_ms;
            gcs().send_text(MAV_SEVERITY_INFO, "%s", stage_msg);
        }
    }

#if HAL_LOGGING_ENABLED
    // log at 10hz or if stage changes
    uint32_t now = AP_HAL::millis();
    if ((stage != prev_stage) || (now - last_log_ms) > 100) {
        prev_stage = stage;
        last_log_ms = now;
        const float velocity = inertial_nav.get_velocity_neu_cms().length();
        const float velocity_z = inertial_nav.get_velocity_z_up_cms();
        const float accel = copter.ins.get_accel().length();
        const float ef_accel_z = ahrs.get_accel_ef().z;
        const bool throw_detect = (stage > Throw_Detecting) || throw_detected();
        const bool attitude_ok = (stage > Throw_Uprighting) || throw_attitude_good();
        const bool height_ok = (stage > Throw_HgtStabilise) || throw_height_good();
        const bool pos_ok = (stage > Throw_PosHold) || throw_position_good();

// @LoggerMessage: THRO
// @Description: Throw Mode messages
// @URL: https://ardupilot.org/copter/docs/throw-mode.html
// @Field: TimeUS: Time since system startup
// @Field: Stage: Current stage of the Throw Mode
// @Field: Vel: Magnitude of the velocity vector
// @Field: VelZ: Vertical Velocity
// @Field: Acc: Magnitude of the vector of the current acceleration
// @Field: AccEfZ: Vertical earth frame accelerometer value
// @Field: Throw: True if a throw has been detected since entering this mode
// @Field: AttOk: True if the vehicle is upright 
// @Field: HgtOk: True if the vehicle is within 50cm of the demanded height
// @Field: PosOk: True if the vehicle is within 50cm of the demanded horizontal position

        AP::logger().WriteStreaming(
            "THRO",
            "TimeUS,Stage,Vel,VelZ,Acc,AccEfZ,Throw,AttOk,HgtOk,PosOk",
            "s-nnoo----",
            "F-0000----",
            "QBffffbbbb",
            AP_HAL::micros64(),
            (uint8_t)stage,
            (double)velocity,
            (double)velocity_z,
            (double)accel,
            (double)ef_accel_z,
            throw_detect,
            attitude_ok,
            height_ok,
            pos_ok);
    }
#endif  // HAL_LOGGING_ENABLED
}

bool ModeThrow::throw_detected()
{
    // Check that we have a valid navigation solution
    nav_filter_status filt_status = inertial_nav.get_filter_status();
    if (!filt_status.flags.attitude || !filt_status.flags.vert_pos) {
        return false;
    }

    // Check for high speed (>500 cm/s)
    bool high_speed = inertial_nav.get_velocity_neu_cms().length_squared() > (THROW_HIGH_SPEED * THROW_HIGH_SPEED);

    // check for upwards or downwards trajectory (airdrop) of 50cm/s
    bool changing_height;
    if (g2.throw_type == ThrowType::Drop) {
        changing_height = inertial_nav.get_velocity_z_up_cms() < -THROW_VERTICAL_SPEED;
    } else {
        changing_height = inertial_nav.get_velocity_z_up_cms() > THROW_VERTICAL_SPEED;
    }

    // Check for freefall.  For drops use the body-frame accelerometer
    // directly — it reads near zero in freefall regardless of EKF state,
    // and ~1g while attached to a carrier aircraft.  For upward throws
    // keep the existing earth-frame check.
    bool free_falling;
    if (g2.throw_type == ThrowType::Drop) {
        free_falling = copter.ins.get_accel().length() < 0.5f * GRAVITY_MSS;
    } else {
        free_falling = ahrs.get_accel_ef().z > -0.25f * GRAVITY_MSS;
    }

    // Check if the accel length is < 1.0g indicating that any throw action is complete and the copter has been released
    bool no_throw_action = copter.ins.get_accel().length() < 1.0f * GRAVITY_MSS;

    // fetch the altitude above home
    float altitude_above_home;  // Use altitude above home if it is set, otherwise relative to EKF origin
    if (ahrs.home_is_set()) {
        ahrs.get_relative_position_D_home(altitude_above_home);
        altitude_above_home = -altitude_above_home; // altitude above home is returned as negative
    } else {
        altitude_above_home = inertial_nav.get_position_z_up_cm() * 0.01f; // centimeters to meters
    }

    // Check that the altitude is within user defined limits
    const bool height_within_params = (g.throw_altitude_min == 0 || altitude_above_home > g.throw_altitude_min) && (g.throw_altitude_max == 0 || (altitude_above_home < g.throw_altitude_max));

    // High velocity or free-fall combined with increasing height indicate a possible air-drop or throw release
    bool possible_throw_detected;
    if (g2.throw_type == ThrowType::Drop) {
        // For drops, body-frame freefall is mandatory — carrier flight
        // speed would otherwise false-trigger via high_speed alone
        possible_throw_detected = free_falling && changing_height && no_throw_action && height_within_params;
    } else {
        possible_throw_detected = (free_falling || high_speed) && changing_height && no_throw_action && height_within_params;
    }

    // For drops, require freefall conditions to persist for a short window
    // to reject transient low-g events (e.g. carrier aircraft maneuvers)
    if (g2.throw_type == ThrowType::Drop) {
        if (possible_throw_detected) {
            if (drop_confirm_start_ms == 0) {
                drop_confirm_start_ms = AP_HAL::millis();
            }
            return (AP_HAL::millis() - drop_confirm_start_ms >= THROW_DROP_CONFIRM_MS);
        }
        drop_confirm_start_ms = 0;
        return false;
    }

    // Record time and vertical velocity when we detect the possible throw
    if (possible_throw_detected && ((AP_HAL::millis() - free_fall_start_ms) > 500)) {
        free_fall_start_ms = AP_HAL::millis();
        free_fall_start_velz = inertial_nav.get_velocity_z_up_cms();
    }

    // Once a possible throw condition has been detected, we check for 2.5 m/s of downwards velocity change in less than 0.5 seconds to confirm
    bool throw_condition_confirmed = ((AP_HAL::millis() - free_fall_start_ms < 500) && ((inertial_nav.get_velocity_z_up_cms() - free_fall_start_velz) < -250.0f));

    // start motors and enter the control mode if we are in continuous freefall
    return throw_condition_confirmed;
}

bool ModeThrow::throw_attitude_good() const
{
    // Check that we have uprighted the copter
    const Matrix3f &rotMat = ahrs.get_rotation_body_to_ned();
    return (rotMat.c.z > 0.866f); // is_upright
}

bool ModeThrow::throw_height_good() const
{
    // Check that we are within 0.5m of the demanded height
    return (pos_control->get_pos_error_z_cm() < 50.0f);
}

bool ModeThrow::throw_velocity_good() const
{
    // Check that vertical velocity is below 50 cm/s
    return (fabsf(inertial_nav.get_velocity_z_up_cms()) < 50.0f);
}

bool ModeThrow::throw_position_good() const
{
    // check that our horizontal position error is within 50cm
    return (pos_control->get_pos_error_xy_cm() < 50.0f);
}

#endif
