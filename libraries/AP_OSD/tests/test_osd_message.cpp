#include <AP_gtest.h>

#include <AP_HAL/AP_HAL.h>
#include <AP_OSD/AP_OSD_config.h>

#if OSD_ENABLED

#include <AP_OSD/AP_OSD_Message.h>
#include <string.h>

const AP_HAL::HAL& hal = AP_HAL::get_HAL();

using namespace AP_OSD_Msg;

// classify() is called on already-upper-cased text (draw_message upper-cases
// the message first), so all fixtures here are upper-case.
TEST(OSDMessage, classify_single)
{
    EXPECT_TRUE(classify("PREARM: WAITING FOR FIX") & CAT_PREARM);
    EXPECT_TRUE(classify("EKF3 IMU0 STOPPED AIDING") & CAT_EKF);
    EXPECT_TRUE(classify("AHRS: DCM ACTIVE") & CAT_EKF);
    EXPECT_TRUE(classify("GPS 1: DETECTED U-BLOX") & CAT_GPS);
    EXPECT_TRUE(classify("GNSS FIX LOST") & CAT_GPS);
    EXPECT_TRUE(classify("BATTERY 1 LOW VOLTAGE") & CAT_BATT);
    EXPECT_TRUE(classify("COMPASS NOT CALIBRATED") & CAT_COMPASS);
    EXPECT_TRUE(classify("MAGNETOMETER UNHEALTHY") & CAT_COMPASS);
    EXPECT_TRUE(classify("THROTTLE FS ACTIVATED") & CAT_RC);
    EXPECT_TRUE(classify("RADIO FAILSAFE") & CAT_RC);
    EXPECT_TRUE(classify("VIBRATION COMPENSATION ON") & CAT_VIBE);
    EXPECT_TRUE(classify("GYROS INCONSISTENT") & CAT_INS);
    EXPECT_TRUE(classify("ACCELS INCONSISTENT") & CAT_INS);
    EXPECT_TRUE(classify("ARMING MOTORS") & CAT_ARM);
    EXPECT_TRUE(classify("DISARMING MOTORS") & CAT_ARM);
    EXPECT_TRUE(classify("CALIBRATING BAROMETER") & CAT_CAL);
    EXPECT_TRUE(classify("3D ACCEL CALIBRATION NEEDED") & CAT_CAL);
    EXPECT_EQ(0, classify("TAKEOFF COMPLETE"));   // matches no category
}

// "CAL" as a bare substring would wrongly match VERTICAL/CRITICAL/LOCAL/SCALE;
// the classifier keys on "CALIBRAT" to avoid that.
TEST(OSDMessage, classify_cal_no_false_positive)
{
    EXPECT_FALSE(classify("VERTICAL POSITION DRIFT") & CAT_CAL);
    EXPECT_FALSE(classify("CRITICAL FAILURE") & CAT_CAL);
    EXPECT_FALSE(classify("LOCAL FRAME") & CAT_CAL);
}

// A PreArm message must NOT be mistaken for an arm/disarm event, even though it
// contains the substring "ARM".
TEST(OSDMessage, classify_prearm_not_arm)
{
    const uint16_t c = classify("PREARM: COMPASS NOT CALIBRATED");
    EXPECT_TRUE(c & CAT_PREARM);
    EXPECT_TRUE(c & CAT_COMPASS);   // may match several categories
    EXPECT_FALSE(c & CAT_ARM);      // but not the arm/disarm event category
}

TEST(OSDMessage, classify_multi)
{
    // "PreArm: GPS: ..." belongs to both PreArm and GPS
    const uint16_t c = classify("PREARM: GPS: WAITING FOR FIX");
    EXPECT_TRUE(c & CAT_PREARM);
    EXPECT_TRUE(c & CAT_GPS);
}

TEST(OSDMessage, should_show_level_filter)
{
    // allow_mask 0 -> category filtering off; only the level filter applies.
    // msg_level = NOTICE(5): show <= 5, hide 6/7. crit_override = CRITICAL(2).
    EXPECT_TRUE(should_show(SEV_WARNING, 0, 0, SEV_NOTICE, SEV_CRITICAL));
    EXPECT_TRUE(should_show(SEV_NOTICE,  0, 0, SEV_NOTICE, SEV_CRITICAL));
    EXPECT_FALSE(should_show(SEV_INFO,   0, 0, SEV_NOTICE, SEV_CRITICAL));
    EXPECT_FALSE(should_show(SEV_DEBUG,  0, 0, SEV_NOTICE, SEV_CRITICAL));
}

TEST(OSDMessage, should_show_allowlist)
{
    // allow only GPS. level filter wide open (DEBUG).
    const uint32_t allow = CAT_GPS;
    EXPECT_TRUE(should_show(SEV_INFO, CAT_GPS, allow, SEV_DEBUG, SEV_CRITICAL));
    // an EKF message (not on the allow-list) at non-critical severity is hidden
    EXPECT_FALSE(should_show(SEV_INFO, CAT_EKF, allow, SEV_DEBUG, SEV_CRITICAL));
    // a message matching no category is hidden when an allow-list is active
    EXPECT_FALSE(should_show(SEV_INFO, 0, allow, SEV_DEBUG, SEV_CRITICAL));
}

TEST(OSDMessage, should_show_critical_override)
{
    // allow only GPS, but a CRITICAL EKF message must still show (safety net)
    const uint32_t allow = CAT_GPS;
    EXPECT_TRUE(should_show(SEV_CRITICAL, CAT_EKF, allow, SEV_INFO, SEV_CRITICAL));
    EXPECT_TRUE(should_show(SEV_EMERGENCY, 0, allow, SEV_INFO, SEV_CRITICAL));
    // the override also beats the level filter
    EXPECT_TRUE(should_show(SEV_ALERT, 0, 0, SEV_EMERGENCY, SEV_CRITICAL));
}

TEST(OSDMessage, style_by_severity)
{
    // emergency/alert/critical -> blink (no invert)
    for (uint8_t sev : {SEV_EMERGENCY, SEV_ALERT, SEV_CRITICAL}) {
        const Style s = style_for(sev);
        EXPECT_TRUE(s.blink);
        EXPECT_FALSE(s.invert);
    }
    // error/warning -> invert (no blink)
    for (uint8_t sev : {SEV_ERROR, SEV_WARNING}) {
        const Style s = style_for(sev);
        EXPECT_FALSE(s.blink);
        EXPECT_TRUE(s.invert);
    }
    // notice/info/debug -> no emphasis
    for (uint8_t sev : {SEV_NOTICE, SEV_INFO, SEV_DEBUG}) {
        const Style s = style_for(sev);
        EXPECT_FALSE(s.blink);
        EXPECT_FALSE(s.invert);
    }
    // colour page stays 0 until HD colour is opted in
    EXPECT_EQ(0, style_for(SEV_EMERGENCY).page);
}

TEST(OSDMessage, str_replace_shortens)
{
    char buf[32];
    strcpy(buf, "BATTERY BATTERY");
    str_replace(buf, "BATTERY", "BAT");
    EXPECT_STREQ("BAT BAT", buf);
}

TEST(OSDMessage, str_replace_never_grows)
{
    char buf[16];
    strcpy(buf, "HI");
    // replacement longer than the key must be ignored (shorten-only invariant)
    str_replace(buf, "HI", "HELLO");
    EXPECT_STREQ("HI", buf);
}

TEST(OSDMessage, abbreviate_dictionary)
{
    char buf[64];
    strcpy(buf, "PREARM: COMPASS NOT CALIBRATED");
    abbreviate(buf);
    // PREARM:->PA:, COMPASS->MAG, CALIBRATED->CAL
    EXPECT_STREQ("PA:MAG NOT CAL", buf);
}

AP_GTEST_PANIC()
AP_GTEST_MAIN()

#else
AP_GTEST_MAIN()
#endif // OSD_ENABLED
