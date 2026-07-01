#include "Copter.h"

// table of user settable parameters
const AP_Param::GroupInfo ModeLand::var_info[] = {

    // @Param: SPD_MS
    // @DisplayName: Land speed
    // @Description: The descent speed for the final stage of landing in m/s
    // @Units: m/s
    // @Range: 0.3 2
    // @Increment: 0.1
    // @User: Standard
    AP_GROUPINFO("SPD_MS", 1, ModeLand, land_speed_ms, LAND_SPD_MS_DEFAULT),

    // @Param: SPD_HIGH_MS
    // @DisplayName: Land speed high
    // @Description: The descent speed for the first stage of landing in m/s. If this is zero then WP_SPD_DN is used
    // @Units: m/s
    // @Range: 0 5
    // @Increment: 0.1
    // @User: Standard
    AP_GROUPINFO("SPD_HIGH_MS", 2, ModeLand, land_speed_high_ms, 0),

    // @Param: ALT_LOW_M
    // @DisplayName: Land alt low
    // @Description: Altitude during Landing at which vehicle slows to LAND_SPD_MS
    // @Units: m
    // @Range: 1 100
    // @Increment: 0.1
    // @User: Advanced
    AP_GROUPINFO("ALT_LOW_M", 3, ModeLand, land_alt_low_m, 10),

    // @Param: FS_OPTIONS
    // @DisplayName: Land failsafe options
    // @Description: Options that apply when LAND is entered because of a failsafe (radio, GCS or EKF), when the pilot cannot intervene. Bit 0 (Advanced land failsafe) enables two protections against a corrupt EKF vertical estimate flying the vehicle away: (1) the vertical throttle is driven from the vibration-resistant controller (feed-forward from the commanded descent plus a heavily gained-down integrator) so a wrong-sign velocity cannot make LAND add throttle to "arrest" a descent that is not happening; (2) a baro-only runaway detector - independent of the EKF - clamps the throttle below hover if the barometer shows a large sustained ascent, so the vehicle physically cannot climb away.
    // @Bitmask: 0:Advanced land failsafe (vibration-resistant throttle + baro runaway cap)
    // @User: Advanced
    AP_GROUPINFO("FS_OPTIONS", 4, ModeLand, fs_options, 0),

    AP_GROUPEND
};

// constructor
ModeLand::ModeLand() : Mode()
{
    // load parameter defaults
    AP_Param::setup_object_defaults(this, var_info);
}

// convert parameters
void ModeLand::convert_params()
{
    // PARAMETER_CONVERSION - Added: Jan 2026

    // return immediately if parameter conversion has already been performed
    if (land_speed_ms.configured() || land_speed_high_ms.configured() || land_alt_low_m.configured()) {
        return;
    }

    static const AP_Param::ConversionInfo conversion_info[] = {
        { Parameters::k_param_land_speed_cms, 0, AP_PARAM_INT16, "LAND_SPD_MS" },     // LAND_SPEED moved to LAND_SPD_MS
        { Parameters::k_param_land_speed_high_cms, 0, AP_PARAM_INT16, "LAND_SPD_HIGH_MS" },   // LAND_SPEED_HIGH moved to LAND_SPD_HIGH_MS
        { Parameters::k_param_g2, 25, AP_PARAM_INT16, "LAND_ALT_LOW_M" },  // LAND_ALT_LOW moved to LAND_ALT_LOW_M
    };
    AP_Param::convert_old_parameters_scaled(conversion_info, ARRAY_SIZE(conversion_info), 0.01, 0);
}

// land_init - initialise land controller
bool ModeLand::init(bool ignore_checks)
{
    // check if we have GPS and decide which LAND we're going to do
    control_position = copter.position_ok();

    // set horizontal speed and acceleration limits
    pos_control->NE_set_max_speed_accel_m(wp_nav->get_default_speed_NE_ms(), wp_nav->get_wp_acceleration_mss());
    pos_control->NE_set_correction_speed_accel_m(wp_nav->get_default_speed_NE_ms(), wp_nav->get_wp_acceleration_mss());

    // initialise the horizontal position controller
    if (control_position && !pos_control->NE_is_active()) {
        pos_control->NE_init_controller();
    }

    // set vertical speed and acceleration limits
    pos_control->D_set_max_speed_accel_m(wp_nav->get_default_speed_down_ms(), wp_nav->get_default_speed_up_ms(), wp_nav->get_accel_D_mss());
    pos_control->D_set_correction_speed_accel_m(wp_nav->get_default_speed_down_ms(), wp_nav->get_default_speed_up_ms(), wp_nav->get_accel_D_mss());

    // initialise the vertical position controller
    if (!pos_control->D_is_active()) {
        pos_control->D_init_controller();
    }

    land_start_time = millis();
    land_pause = false;

    // reset advanced land failsafe state
    adv_fs_active = false;
    adv_fs_throttle_capped = false;

    // reset flag indicating if pilot has applied roll or pitch inputs during landing
    copter.ap.land_repo_active = false;

    // this will be set true if prec land is later active
    copter.ap.prec_land_active = false;

    // initialise yaw
    auto_yaw.set_mode(AutoYaw::Mode::HOLD);

#if AP_LANDINGGEAR_ENABLED
    // optionally deploy landing gear
    copter.landinggear.deploy_for_landing();
#endif

#if AC_PRECLAND_ENABLED
    // initialise precland state machine
    copter.precland_statemachine.init();
#endif

    return true;
}

// Advanced land failsafe tuning (LAND_FS_OPTIONS bit 0)
// Baro net ascent that confirms a runaway climb.  Deliberately large: this
// backstop exists to stop a hundreds-of-metres fly-away, not to police small
// excursions, so it sits well above any near-ground baro / ground-effect noise
// and a false trip is very unlikely.
#define LAND_FS_RUNAWAY_CLIMB_M   10.0f
// Throttle ceiling once a runaway is confirmed, as a fraction of hover throttle.
// Below hover so the vehicle descends regardless of the (corrupt) estimate.
#define LAND_FS_THROTTLE_CAP      0.90f

// land_run - runs the land controller
// should be called at 100hz or more
void ModeLand::run()
{
    update_advanced_failsafe();

    if (control_position) {
        gps_run();
    } else {
        nogps_run();
    }

    // clamp the throttle AFTER the vertical controller has set it
    apply_advanced_failsafe_throttle_cap();
}

// update_advanced_failsafe - engage the failsafe-landing protections (bit 0).
// During a failsafe-triggered landing the pilot cannot intervene, so:
//  - drive the vertical throttle from the vibration-resistant law so a corrupt
//    EKF vertical velocity cannot make LAND add throttle and climb away; and
//  - watch the barometer (independent of the EKF) for a large sustained ascent
//    that means the vehicle is running away despite being told to land.
void ModeLand::update_advanced_failsafe()
{
    // a failsafe that removes the pilot or corrupts the vertical estimate
    const bool failsafe_active = copter.failsafe.radio ||
                                 copter.failsafe.gcs ||
                                 copter.failsafe.ekf;
    const bool armed = option_is_enabled(Option::AdvancedFailsafe) && failsafe_active;

    // vibration-resistant throttle: OR in the vibration-failsafe detector's
    // request so we never clear a genuine compensation owned by check_vibration()
    pos_control->set_vibe_comp(armed || copter.vibration_check.high_vibes);

    if (!armed) {
        adv_fs_active = false;
        adv_fs_throttle_capped = false;
        return;
    }

    // baro-only runaway-climb detector.  LAND only ever commands a descent, so
    // any large net ascent of the barometer is a fly-away regardless of what the
    // (corrupt) EKF vertical velocity says.
    const float baro_alt_m = copter.barometer.get_altitude();
    if (!adv_fs_active) {
        // rising edge: start measuring the ascent from here
        adv_fs_active = true;
        adv_fs_baro_alt_min_m = baro_alt_m;
        adv_fs_throttle_capped = false;
    }
    adv_fs_baro_alt_min_m = MIN(adv_fs_baro_alt_min_m, baro_alt_m);
    if (baro_alt_m - adv_fs_baro_alt_min_m > LAND_FS_RUNAWAY_CLIMB_M) {
        // latched for the rest of the failsafe landing so the vehicle is brought down
        if (!adv_fs_throttle_capped) {
            gcs().send_text(MAV_SEVERITY_CRITICAL, "Land FS: baro runaway, capping throttle");
        }
        adv_fs_throttle_capped = true;
    }
}

// apply_advanced_failsafe_throttle_cap - hard throttle ceiling once a baro-
// confirmed runaway climb has latched.  Runs after the vertical controller so it
// clamps the actual commanded throttle: below hover the vehicle cannot sustain a
// climb whatever the (corrupt) vertical estimate demands.
void ModeLand::apply_advanced_failsafe_throttle_cap()
{
    if (!adv_fs_throttle_capped) {
        return;
    }
    const float cap = motors->get_throttle_hover() * LAND_FS_THROTTLE_CAP;
    if (attitude_control->get_throttle_in() > cap) {
        attitude_control->set_throttle_out(cap, true, copter.g.throttle_filt);
    }
}

// hand vibration compensation back to the vibration-failsafe detector and clear
// the advanced-failsafe state so leaving LAND does not strand either protection
void ModeLand::exit()
{
    pos_control->set_vibe_comp(copter.vibration_check.high_vibes);
    adv_fs_active = false;
    adv_fs_throttle_capped = false;
}

// land_gps_run - runs the land controller
//      horizontal position controlled with loiter controller
//      should be called at 100hz or more
void ModeLand::gps_run()
{
    // disarm when the landing detector says we've landed
    if (copter.ap.land_complete && motors->get_spool_state() == AP_Motors::SpoolState::GROUND_IDLE) {
        copter.arming.disarm(AP_Arming::Method::LANDED);
    }

    // Land State Machine Determination
    if (is_disarmed_or_landed()) {
        make_safe_ground_handling();
    } else {
        // set motors to full range
        motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::THROTTLE_UNLIMITED);

        // pause before beginning land descent
        if (land_pause && millis()-land_start_time >= LAND_WITH_DELAY_MS) {
            land_pause = false;
        }

        // run normal landing or precision landing (if enabled)
        land_run_normal_or_precland(land_pause);
    }
}

// land_nogps_run - runs the land controller
//      pilot controls roll and pitch angles
//      should be called at 100hz or more
void ModeLand::nogps_run()
{
    float target_roll_rad = 0.0f, target_pitch_rad = 0.0f;

    // process pilot inputs
    if (rc().has_valid_input()) {
        if ((g.throttle_behavior & THR_BEHAVE_HIGH_THROTTLE_CANCELS_LAND) != 0 && copter.rc_throttle_control_in_filter.get() > LAND_CANCEL_TRIGGER_THR){
            LOGGER_WRITE_EVENT(LogEvent::LAND_CANCELLED_BY_PILOT);
            // exit land if throttle is high
            copter.set_mode(Mode::Number::ALT_HOLD, ModeReason::THROTTLE_LAND_ESCAPE);
        }

        if (g.land_repositioning) {
            // apply SIMPLE mode transform to pilot inputs
            update_simple_mode();

            // get pilot desired lean angles
            get_pilot_desired_lean_angles_rad(target_roll_rad, target_pitch_rad, attitude_control->lean_angle_max_rad(), attitude_control->get_althold_lean_angle_max_rad());
        }
    }

    // disarm when the landing detector says we've landed
    if (copter.ap.land_complete && motors->get_spool_state() == AP_Motors::SpoolState::GROUND_IDLE) {
        copter.arming.disarm(AP_Arming::Method::LANDED);
    }

    // Land State Machine Determination
    if (is_disarmed_or_landed()) {
        make_safe_ground_handling();
    } else {
        // set motors to full range
        motors->set_desired_spool_state(AP_Motors::DesiredSpoolState::THROTTLE_UNLIMITED);

        // pause before beginning land descent
        if (land_pause && millis()-land_start_time >= LAND_WITH_DELAY_MS) {
            land_pause = false;
        }

        land_run_vertical_control(land_pause);
    }

    // call attitude controller
    attitude_control->input_euler_angle_roll_pitch_euler_rate_yaw_rad(target_roll_rad, target_pitch_rad, auto_yaw.get_heading().yaw_rate_rads);
}

// do_not_use_GPS - forces land-mode to not use the GPS but instead rely on pilot input for roll and pitch
//  called during GPS failsafe to ensure that if we were already in LAND mode that we do not use the GPS
//  has no effect if we are not already in LAND mode
void ModeLand::do_not_use_GPS()
{
    control_position = false;
}

// returns true if pilot's yaw input should be used to adjust vehicle's heading
bool ModeLand::use_pilot_yaw() const
{
    // only accept yaw input if repositioning is enabled
    return g.land_repositioning;
}

// set_mode_land_with_pause - sets mode to LAND and triggers 4 second delay before descent starts
//  this is always called from a failsafe so we trigger notification to pilot
void Copter::set_mode_land_with_pause(ModeReason reason)
{
    set_mode(Mode::Number::LAND, reason);
    mode_land.set_land_pause(true);

    // alert pilot to mode change
    AP_Notify::events.failsafe_mode_change = 1;
}

// landing_with_GPS - returns true if vehicle is landing using GPS
bool Copter::landing_with_GPS()
{
    return (flightmode->mode_number() == Mode::Number::LAND &&
            mode_land.controlling_position());
}
