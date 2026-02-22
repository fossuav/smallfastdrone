#include "Copter.h"

#if MODE_VALT_ENABLED

/*
 * VALT (velocity alt hold) flight mode.
 *
 * Inherits from AltHold, overriding only the Flying state.  Surface
 * tracking is skipped so its offsets do not fight pilot stick input,
 * and pos_desired is overridden with the current position when the
 * stick is off-centre so the position P loop is bypassed.  When the
 * stick returns to centre pos_desired freezes and position P gently
 * holds height.
 */

// velocity-controlled Flying state
void ModeVelAltHold::alt_hold_run_flying(float &target_roll, float &target_pitch, float target_climb_rate)
{
    // get avoidance adjusted climb rate
    target_climb_rate = get_avoidance_adjusted_climbrate(target_climb_rate);

    // Send the commanded climb rate to the position controller
    pos_control->set_pos_target_z_from_climb_rate_cm(target_climb_rate);

    // Override pos_desired with the current position so the position P
    // loop does not fight the pilot's stick input.  When the stick
    // returns to centre (zero climb rate), stop overriding so that
    // pos_desired freezes at the current altitude and position P gently
    // holds height.
    if (!is_zero(target_climb_rate)) {
        pos_control->set_pos_desired_z_cm(inertial_nav.get_position_z_up_cm());
    }
}

#endif  // MODE_VALT_ENABLED
