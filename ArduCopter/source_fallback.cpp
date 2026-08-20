#include "Copter.h"

#if AP_OPTICALFLOW_ENABLED

/*
 * Navigation source fallback monitor.
 *
 * With per-core EKF source sets (EK3_SRC_OPTIONS bit 3) lane 0 runs the
 * GPS sources (EK3_SRC1) and lane 1 the optical flow sources (EK3_SRC2),
 * and automatic lane selection is disabled (EK3_OPTIONS bit 1). This
 * monitor commands the primary lane instead: on GPS loss or detected GPS
 * spoofing it moves flight onto the flow lane before the EKF failsafe can
 * trip, returns to GPS after a plain loss recovers, and if neither lane
 * can provide position while in Loiter changes the vehicle to AltHold.
 *
 * Spoofing is detected by comparing the two lanes: a spoofed GPS drags
 * the GPS lane's states while the flow lane keeps measuring real motion,
 * so their velocity solutions diverge (position-only spoofer) or their
 * positions diverge at a sustained rate (a spoofer whose reported
 * velocity is consistent with its position walk). Quality-based GPS
 * checks cannot see either case. A spoof detection latches GPS as
 * untrusted until disarm or a pilot source set change.
 *
 * SRCF_POSD_NSIG adds a third test on the accumulated position offset,
 * which catches a walk slow enough to sit inside both rate thresholds.
 * It is off by default and has not been flown.
 */

#define SRCF_GPS_BAD_ITERATIONS     3       // 0.3s at 10hz to confirm GPS loss
#define SRCF_FLOW_BAD_ITERATIONS    5       // 0.5s at 10hz to confirm flow loss
#define SRCF_GROUND_LANE_ITERATIONS 20      // 2s at 10hz to change the lane armed on
#define SRCF_POST_SWITCH_MUTE_MS    5000    // divergence detector mute after a lane change
#define SRCF_POS_RATE_WINDOW        20      // position divergence rate baseline, 2s at 10hz
// Sigma bound on the cross-lane position offset before recovery is allowed.
// Field logs 332-335 put honest recoveries under 1.8 sigma and the worst
// flow-lane drift, 32m over 171s, at 4.0; a 500m capture is about 125. Loose
// on purpose - it separates tens of metres from hundreds, and blocking a
// legitimate recovery is worse than missing a small static offset.
#define SRCF_RECOV_POS_NSIGMA       6.0f
// Floor under the offset detector's denominator. The flow lane only earns
// position uncertainty by dead reckoning, so just after takeoff the divisor
// is smallest exactly while the lanes are still settling: field log 57 pairs
// a 2.4m offset with a 0.79m sigma at 0.2m AGL, a ratio of 3.1 against an
// airborne worst of 2.05. Without a floor the gate collapses on the ground
// and the detector is most likely to false trip where it is least useful.
#define SRCF_POSD_MIN_SIGMA         2.0f

static const uint8_t SRCF_GPS_LANE = 0;     // lane running EK3_SRC1 (GPS)
static const uint8_t SRCF_FLOW_LANE = 1;    // lane running EK3_SRC2 (optical flow)

enum class LaneState : uint8_t {
    GPS_PRIMARY = 0,    // flying on the GPS lane
    FLOW_LOSS = 1,      // on the flow lane after GPS loss, may auto-recover
    FLOW_SPOOF = 2,     // on the flow lane after spoof detection, latched
    FLOW_NO_GPS = 3,    // armed on the flow lane, GPS not yet acquired this flight
};

static struct {
    LaneState lane_state;
    bool gps_untrusted;         // spoof latch, cleared on disarm or pilot source set change
    bool switch_warned;         // lane switch command failed warning sent
    bool align_pending;         // pull the flow lane into the GPS lane's frame once the switch lands
    bool origin_before_arming;  // an origin existed before takeoff, so both lanes share an earth frame
    uint8_t ground_lane_count;  // consecutive ticks the armed-on lane choice has differed
    uint8_t gps_bad_count;
    uint8_t flow_bad_count;
    uint16_t vel_vote;          // velocity divergence confirmation vote
    uint16_t pos_vote;          // position divergence rate confirmation vote
    uint16_t pos_off_vote;      // position offset confirmation vote
    float pos_div_hist[SRCF_POS_RATE_WINDOW];   // ring buffer of position divergence
    uint8_t hist_idx;
    uint8_t hist_count;
    uint32_t recovery_start_ms; // time GPS recovery conditions first held
    uint32_t offset_block_ms;   // time the offset bound alone first blocked recovery
    bool offset_warned;         // offset block warning sent for this episode
    uint32_t last_lane_cmd_ms;  // time of last commanded lane change
    uint8_t last_source_set;    // detects a pilot source set change
} srcf_state;

// reset the transient detectors, keeping the latches
static void srcf_reset_detectors()
{
    srcf_state.gps_bad_count = 0;
    srcf_state.flow_bad_count = 0;
    srcf_state.vel_vote = 0;
    srcf_state.pos_vote = 0;
    srcf_state.pos_off_vote = 0;
    srcf_state.hist_idx = 0;
    srcf_state.hist_count = 0;
    srcf_state.recovery_start_ms = 0;
    srcf_state.offset_block_ms = 0;
    srcf_state.offset_warned = false;
}

// A receiver sitting in the wrong place is otherwise indistinguishable from
// one that never came back: nothing is sent and GPS simply never arrives.
// Say so once the offset bound alone has held the handover off for as long as
// the handover itself would have taken, so the message means "this would have
// switched by now".
static void srcf_offset_block_warn(uint32_t now_ms, uint32_t hold_ms, float pos_div, const char *how)
{
    if (srcf_state.offset_block_ms == 0) {
        srcf_state.offset_block_ms = now_ms;
    } else if (!srcf_state.offset_warned &&
               (now_ms - srcf_state.offset_block_ms > hold_ms)) {
        srcf_state.offset_warned = true;
        gcs().send_text(MAV_SEVERITY_WARNING, "SRCF: GPS %s %.0fm off, staying on flow",
                        how, (double)pos_div);
    }
}

// command the EKF primary lane and arm the failsafe holdoff
bool Copter::source_fallback_command_lane(uint8_t lane)
{
    if (!ahrs.set_ekf_primary_lane(lane)) {
        if (!srcf_state.switch_warned) {
            srcf_state.switch_warned = true;
            gcs().send_text(MAV_SEVERITY_WARNING, "SRCF: lane switch unavailable");
        }
        return false;
    }
    srcf_state.last_lane_cmd_ms = AP_HAL::millis();
    srcf_reset_detectors();
    reset_ekf_check_gate();
    return true;
}

// choose the lane to arm on. Normally the GPS lane, but at SRCF_ENABLE=2 a
// vehicle that cannot see GPS arms on the flow lane instead. The arming GPS
// checks are keyed on the primary lane's configured sources
// (AP_Arming_Copter.cpp:405 via AP_AHRS::using_gps), so moving the primary is
// what stands them down - there is nothing to relax in AP_Arming itself.
void Copter::source_fallback_ground_lane()
{
    bool gps_lane_healthy = false;
    bool flow_lane_healthy = false;
    nav_filter_status gps_lane_status {};
    nav_filter_status flow_lane_status {};
    if (!ahrs.get_lane_status(SRCF_GPS_LANE, gps_lane_healthy, gps_lane_status) ||
        !ahrs.get_lane_status(SRCF_FLOW_LANE, flow_lane_healthy, flow_lane_status)) {
        return;
    }

    // predicted position counts while disarmed, matching Copter::position_ok
    const bool gps_lane_ready = gps_lane_healthy &&
                                (gps_lane_status.flags.horiz_pos_abs || gps_lane_status.flags.pred_horiz_pos_abs);
    const bool flow_lane_ready = flow_lane_healthy && optflow.healthy() &&
                                 (flow_lane_status.flags.horiz_pos_rel || flow_lane_status.flags.pred_horiz_pos_rel);

    uint8_t want_lane = SRCF_GPS_LANE;
    if ((g2.srcf_enable >= 2) && !gps_lane_ready && flow_lane_ready) {
        want_lane = SRCF_FLOW_LANE;
    }

    const int8_t primary = ahrs.get_primary_core_index();
    if (primary >= 0 && want_lane != primary) {
        // a marginal indoor fix that comes and goes would otherwise flap the
        // primary, and the EKF announces every change at CRITICAL
        if (++srcf_state.ground_lane_count >= SRCF_GROUND_LANE_ITERATIONS) {
            srcf_state.ground_lane_count = 0;
            if (source_fallback_command_lane(want_lane) && want_lane == SRCF_FLOW_LANE) {
                gcs().send_text(MAV_SEVERITY_INFO, "SRCF: no GPS, arming on flow lane");
            }
        }
    } else {
        srcf_state.ground_lane_count = 0;
    }

    // track the lane actually in use rather than the one asked for: a
    // commanded switch does not land until the EKF's next update
    srcf_state.lane_state = (ahrs.get_primary_core_index() == SRCF_FLOW_LANE) ?
                            LaneState::FLOW_NO_GPS : LaneState::GPS_PRIMARY;
}

// true while the primary lane's absolute position is provisional: the vehicle
// armed on the flow lane and the GPS lane has not been taken up yet, so the
// two are still offset by however far the vehicle has travelled since arming
bool Copter::source_fallback_position_provisional() const
{
    return (g2.srcf_enable > 0) && (srcf_state.lane_state == LaneState::FLOW_NO_GPS);
}

// monitor navigation sources and manage the GPS -> flow -> AltHold ladder
// called at 10hz, ahead of ekf_check() so holdoffs land the same tick
void Copter::source_fallback_update()
{
    if (g2.srcf_enable <= 0) {
        return;
    }

    const uint32_t now_ms = AP_HAL::millis();

    if (!motors->armed()) {
        // pick the lane for the next flight and clear the latches
        source_fallback_ground_lane();
        srcf_state.gps_untrusted = false;
        srcf_state.switch_warned = false;
        srcf_state.align_pending = false;
        // an origin that predates takeoff puts the flow lane in a real earth
        // frame rather than one referenced to wherever it began aiding, which
        // is what makes the cross-lane offset testable at the first fix
        Location origin;
        srcf_state.origin_before_arming = ahrs.get_origin(origin);
        srcf_reset_detectors();
        srcf_state.last_source_set = ahrs.get_posvelyaw_source_set();
        return;
    }

    // per-lane health; inert until both EKF lanes are allocated
    bool gps_lane_healthy = false;
    bool flow_lane_healthy = false;
    nav_filter_status gps_lane_status {};
    nav_filter_status flow_lane_status {};
    if (!ahrs.get_lane_status(SRCF_GPS_LANE, gps_lane_healthy, gps_lane_status) ||
        !ahrs.get_lane_status(SRCF_FLOW_LANE, flow_lane_healthy, flow_lane_status)) {
        return;
    }

    // a pilot source set change is inert under per-core sources, so reuse
    // it as the explicit "trust GPS again" action after a spoof latch
    const uint8_t source_set = ahrs.get_posvelyaw_source_set();
    if (source_set != srcf_state.last_source_set) {
        srcf_state.last_source_set = source_set;
        if (srcf_state.gps_untrusted) {
            srcf_state.gps_untrusted = false;
            if (srcf_state.lane_state == LaneState::FLOW_SPOOF) {
                srcf_state.lane_state = LaneState::FLOW_LOSS;
            }
            gcs().send_text(MAV_SEVERITY_INFO, "SRCF: GPS trust reset");
        }
    }

    // GPS receiver loss, confirmed over SRCF_GPS_BAD_ITERATIONS
    const bool gps_bad_now = (gps.status() < AP_GPS::GPS_OK_FIX_3D) ||
                             (now_ms - gps.last_message_time_ms() > 1000);
    srcf_state.gps_bad_count = gps_bad_now ? MIN(srcf_state.gps_bad_count + 1, SRCF_GPS_BAD_ITERATIONS) : 0;
    const bool gps_bad = srcf_state.gps_bad_count >= SRCF_GPS_BAD_ITERATIONS;

    // flow lane usability: healthy, providing relative position, sensor alive
    const bool flow_usable = flow_lane_healthy && flow_lane_status.flags.horiz_pos_rel && optflow.healthy();
    srcf_state.flow_bad_count = flow_usable ? 0 : MIN(srcf_state.flow_bad_count + 1, SRCF_FLOW_BAD_ITERATIONS);
    const bool flow_bad = srcf_state.flow_bad_count >= SRCF_FLOW_BAD_ITERATIONS;

    // GPS lane usability: healthy and fusing GPS with absolute position
    const bool gps_lane_usable = gps_lane_healthy && gps_lane_status.flags.horiz_pos_abs && !gps_bad_now;

    // cross-lane divergence against whichever lane is not primary. The
    // detector is muted after a lane change while divergence changes meaning
    const int8_t primary = ahrs.get_primary_core_index();
    const uint8_t other_lane = (primary == SRCF_GPS_LANE) ? SRCF_FLOW_LANE : SRCF_GPS_LANE;
    const bool muted = (srcf_state.last_lane_cmd_ms != 0) &&
                       (now_ms - srcf_state.last_lane_cmd_ms < SRCF_POST_SWITCH_MUTE_MS);

    // a lane switch is not applied until the EKF's next update, and the flow
    // lane cannot be shifted while it is the one flying the vehicle, so the
    // alignment waits for the commanded switch to land
    if (srcf_state.align_pending && primary == SRCF_GPS_LANE) {
        srcf_state.align_pending = false;
        ahrs.align_lane_position(SRCF_FLOW_LANE);
    }

    float vel_div = 0.0f;
    float pos_div = 0.0f;
    float pos_rate = 0.0f;
    bool pos_rate_valid = false;
    const bool div_ok = !muted && ahrs.get_lane_divergence(other_lane, vel_div, pos_div);
    if (div_ok) {
        if (srcf_state.hist_count >= SRCF_POS_RATE_WINDOW) {
            const float oldest = srcf_state.pos_div_hist[srcf_state.hist_idx];
            pos_rate = (pos_div - oldest) / (SRCF_POS_RATE_WINDOW * 0.1f);
            pos_rate_valid = true;
        }
        srcf_state.pos_div_hist[srcf_state.hist_idx] = pos_div;
        srcf_state.hist_idx = (srcf_state.hist_idx + 1) % SRCF_POS_RATE_WINDOW;
        if (srcf_state.hist_count < SRCF_POS_RATE_WINDOW) {
            srcf_state.hist_count++;
        }
    } else {
        srcf_state.hist_idx = 0;
        srcf_state.hist_count = 0;
    }

    // A divergence only means something measured against how well the two
    // lanes claim to know their own velocity. The flow lane's velocity comes
    // from flow rate times height, so its uncertainty grows with height and a
    // fixed threshold that suits a low hover reads as a spoof at altitude.
    // The same scale applies to the position rate: it is a position
    // difference differenced over time, so it is a velocity, and the lanes'
    // unbounded position uncertainty is not the scale to judge it against.
    float vel_sigma = 0.0f;
    const bool sigma_valid = is_positive(g2.srcf_nsigma) &&
                             ahrs.get_lane_divergence_sigma(other_lane, vel_sigma);
    const float sig_gate = sigma_valid ? (g2.srcf_nsigma * vel_sigma) : 0.0f;
    const float vel_gate = MAX(g2.srcf_vel_thr.get(), sig_gate);
    const float pos_gate = MAX(g2.srcf_posr_thr.get(), sig_gate);

    // The offset detector judges the position difference itself against the
    // lanes' combined position uncertainty. That uncertainty accumulates from
    // the same dead reckoning error that opens the offset, so the ratio is
    // self-normalising where the velocity difference is not: field logs 53,
    // 55 and 57 read 1.51, 0.55 and 1.64 across 0 to 8.8 m/s, while vel_div
    // over the same range went 1.26 to 2.17. Off by default, unflown.
    float pos_sigma = 0.0f;
    const bool pos_sigma_valid = ahrs.get_lane_divergence_pos_sigma(other_lane, pos_sigma) &&
                                 is_positive(pos_sigma);

    // one vote integrator per signal: a divergence must persist on the same
    // signal for SRCF_CNF_TIME. A single counter fed by both lets a decaying
    // signal hand over to a rising one and confirm on neither alone.
    // Only meaningful while flying on the GPS lane with a live receiver
    const uint16_t vote_max = MAX(1, (int)(g2.srcf_cnf_time * 10));
    const bool can_vote = div_ok && !gps_bad_now && (primary == SRCF_GPS_LANE);
    if (can_vote && (vel_div > vel_gate)) {
        srcf_state.vel_vote = MIN(srcf_state.vel_vote + 1, vote_max);
    } else if (srcf_state.vel_vote > 0) {
        srcf_state.vel_vote--;
    }
    if (can_vote && pos_rate_valid && (pos_rate > pos_gate)) {
        srcf_state.pos_vote = MIN(srcf_state.pos_vote + 1, vote_max);
    } else if (srcf_state.pos_vote > 0) {
        srcf_state.pos_vote--;
    }
    if (can_vote && pos_sigma_valid && is_positive(g2.srcf_posd_nsig) &&
        (pos_div > g2.srcf_posd_nsig * MAX(pos_sigma, SRCF_POSD_MIN_SIGMA))) {
        srcf_state.pos_off_vote = MIN(srcf_state.pos_off_vote + 1, vote_max);
    } else if (srcf_state.pos_off_vote > 0) {
        srcf_state.pos_off_vote--;
    }

    bool demote_needed = false;

    switch (srcf_state.lane_state) {
    case LaneState::GPS_PRIMARY: {
        const bool spoof_confirmed = (srcf_state.vel_vote >= vote_max) ||
                                     (srcf_state.pos_vote >= vote_max) ||
                                     (srcf_state.pos_off_vote >= vote_max);
        if ((spoof_confirmed || gps_bad) && flow_usable) {
            if (source_fallback_command_lane(SRCF_FLOW_LANE)) {
                if (spoof_confirmed) {
                    srcf_state.gps_untrusted = true;
                    srcf_state.lane_state = LaneState::FLOW_SPOOF;
                    gcs().send_text(MAV_SEVERITY_CRITICAL, "SRCF: GPS spoof suspected, using flow lane");
                } else {
                    srcf_state.lane_state = LaneState::FLOW_LOSS;
                    gcs().send_text(MAV_SEVERITY_CRITICAL, "SRCF: GPS lost, using flow lane");
                }
            }
        } else if (gps_bad && !flow_usable) {
            demote_needed = true;
        }
        break;
    }
    case LaneState::FLOW_LOSS:
    case LaneState::FLOW_SPOOF:
    case LaneState::FLOW_NO_GPS:
        if (flow_bad) {
            // flow lane lost while it carries the vehicle: return to a
            // usable, trusted GPS lane immediately, else give up position
            if (gps_lane_usable && !srcf_state.gps_untrusted) {
                const bool first_fix = (srcf_state.lane_state == LaneState::FLOW_NO_GPS);
                if (source_fallback_command_lane(SRCF_GPS_LANE)) {
                    srcf_state.align_pending = first_fix;
                    srcf_state.lane_state = LaneState::GPS_PRIMARY;
                    gcs().send_text(MAV_SEVERITY_CRITICAL, "SRCF: flow lost, back on GPS lane");
                }
            } else {
                demote_needed = true;
            }
            break;
        }
        if (srcf_state.lane_state == LaneState::FLOW_NO_GPS) {
            // First acquisition, not a recovery. Whether the cross-lane
            // offset means anything depends on where the flow lane's frame
            // came from. Armed with no origin at all it is referenced to
            // wherever relative aiding began while the GPS lane is referenced
            // to the origin it has just set, so the difference is the
            // distance flown since arming and the bound would reject an
            // honest handover outright. With an origin set before takeoff the
            // two share an earth frame and the offset is a real disagreement:
            // field log 346 armed indoors on a recorded origin, held the
            // lanes 25.8m apart at 10-15 sigma for the whole hold on a GPS
            // repeater, took the handover unchallenged and flew into a wall
            // 2.1s later. The velocity and rate detectors saw none of it.
            const bool offset_ok = !srcf_state.origin_before_arming ||
                                   (div_ok && pos_sigma_valid &&
                                    (pos_div < SRCF_RECOV_POS_NSIGMA * pos_sigma));

            if (gps_lane_usable && div_ok && !offset_ok) {
                srcf_offset_block_warn(now_ms, (uint32_t)(g2.srcf_recov_time * 1000), pos_div, "acquired");
            } else {
                srcf_state.offset_block_ms = 0;
            }

            if (gps_lane_usable && offset_ok) {
                if (srcf_state.recovery_start_ms == 0) {
                    srcf_state.recovery_start_ms = now_ms;
                }
                if (now_ms - srcf_state.recovery_start_ms > (uint32_t)(g2.srcf_recov_time * 1000)) {
                    if (source_fallback_command_lane(SRCF_GPS_LANE)) {
                        srcf_state.align_pending = true;
                        srcf_state.lane_state = LaneState::GPS_PRIMARY;
                        gcs().send_text(MAV_SEVERITY_CRITICAL, "SRCF: GPS acquired, using GPS lane");
                    }
                }
            } else {
                srcf_state.recovery_start_ms = 0;
            }
        } else if (srcf_state.lane_state == LaneState::FLOW_LOSS) {
            // A receiver captured onto a static spoof reports a fixed
            // position with no motion, so it presents neither a velocity
            // difference nor a divergence rate and passes both gates above
            // while sitting an arbitrary distance away. Bound the offset
            // itself against how far the two lanes could honestly be apart,
            // which is their combined position uncertainty: that grows as
            // the flow lane dead reckons, so a fixed metre limit would block
            // legitimate recovery on a long outage.
            const bool offset_ok = pos_sigma_valid &&
                                   (pos_div < SRCF_RECOV_POS_NSIGMA * pos_sigma);

            // auto-recovery: GPS lane must be continuously usable and
            // consistent with the flow lane for SRCF_RECOV_TIME. Judged
            // against the same gates as the outbound trip, else at altitude
            // the flow lane's own imprecision blocks recovery indefinitely
            const bool consistent = gps_lane_usable && pos_rate_valid &&
                                    (vel_div < vel_gate) && (fabsf(pos_rate) < pos_gate);
            const bool recovery_ok = consistent && offset_ok;

            if (consistent && !offset_ok) {
                srcf_offset_block_warn(now_ms, (uint32_t)(g2.srcf_recov_time * 1000), pos_div, "returned");
            } else {
                srcf_state.offset_block_ms = 0;
            }

            if (recovery_ok) {
                if (srcf_state.recovery_start_ms == 0) {
                    srcf_state.recovery_start_ms = now_ms;
                }
                if (now_ms - srcf_state.recovery_start_ms > (uint32_t)(g2.srcf_recov_time * 1000)) {
                    if (source_fallback_command_lane(SRCF_GPS_LANE)) {
                        srcf_state.lane_state = LaneState::GPS_PRIMARY;
                        gcs().send_text(MAV_SEVERITY_INFO, "SRCF: GPS recovered");
                    }
                }
            } else {
                srcf_state.recovery_start_ms = 0;
            }
        }
        break;
    }

    // final rung: no lane can provide position. Loiter and Brake are
    // demoted (Brake is where an RC failsafe parks the vehicle); the
    // holdoff stops the EKF failsafe pre-empting the mode change
    if (demote_needed && (flightmode->mode_number() == Mode::Number::LOITER ||
                          flightmode->mode_number() == Mode::Number::BRAKE)) {
        reset_ekf_check_gate();
        if (set_mode(Mode::Number::ALT_HOLD, ModeReason::SOURCE_FALLBACK)) {
            gcs().send_text(MAV_SEVERITY_CRITICAL, "SRCF: no nav source, AltHold");
        }
    }

#if HAL_LOGGING_ENABLED
    // @LoggerMessage: SRCF
    // @Description: Navigation source fallback monitor state
    // @Field: TimeUS: Time since system startup
    // @Field: St: lane state, 0:GPS primary 1:flow after GPS loss 2:flow after spoof 3:flow before GPS acquired
    // @Field: GU: GPS untrusted latch
    // @Field: VD: cross-lane horizontal velocity difference
    // @Field: PD: cross-lane horizontal position difference
    // @Field: PR: cross-lane position difference growth rate
    // @Field: VVot: velocity divergence confirmation vote count
    // @Field: PVot: position divergence rate confirmation vote count
    // @Field: OVot: position offset confirmation vote count
    // @Field: VSig: combined 1-sigma horizontal velocity uncertainty of both lanes
    // @Field: PSig: combined 1-sigma horizontal position uncertainty of both lanes
    // @Field: GpsB: GPS receiver loss confirmed
    // @Field: FlwU: flow lane usable
    // @Field: GpsL: GPS lane usable
    AP::logger().WriteStreaming("SRCF", "TimeUS,St,GU,VD,PD,PR,VVot,PVot,OVot,VSig,PSig,GpsB,FlwU,GpsL", "QBBfffHHHffBBB",
                                AP_HAL::micros64(),
                                (uint8_t)srcf_state.lane_state,
                                (uint8_t)srcf_state.gps_untrusted,
                                (double)vel_div, (double)pos_div, (double)pos_rate,
                                (uint16_t)srcf_state.vel_vote,
                                (uint16_t)srcf_state.pos_vote,
                                (uint16_t)srcf_state.pos_off_vote,
                                (double)vel_sigma,
                                (double)pos_sigma,
                                (uint8_t)gps_bad,
                                (uint8_t)flow_usable,
                                (uint8_t)gps_lane_usable);
#endif
}

#endif  // AP_OPTICALFLOW_ENABLED
