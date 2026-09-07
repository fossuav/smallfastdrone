/*
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "AP_OSD_Message.h"

#if OSD_ENABLED

#include <string.h>

namespace AP_OSD_Msg {

// true if `hay` contains the substring `needle`
static bool has(const char *hay, const char *needle)
{
    return strstr(hay, needle) != nullptr;
}

uint16_t classify(const char *upper)
{
    uint16_t cat = 0;
    if (upper == nullptr) {
        return 0;
    }
    // PreArm is matched explicitly so it does not fall into CAT_ARM below
    if (has(upper, "PREARM") || has(upper, "PA:")) {
        cat |= CAT_PREARM;
    }
    if (has(upper, "EKF") || has(upper, "AHRS") || has(upper, "DCM")) {
        cat |= CAT_EKF;
    }
    if (has(upper, "GPS") || has(upper, "GNSS")) {
        cat |= CAT_GPS;
    }
    if (has(upper, "BATT") || has(upper, "VOLT") || has(upper, "POWER") ||
        has(upper, "CURRENT") || has(upper, "CAPACITY")) {
        cat |= CAT_BATT;
    }
    if (has(upper, "COMPASS") || has(upper, "MAG ") || has(upper, "MAG:") ||
        has(upper, "MAGNET")) {
        cat |= CAT_COMPASS;
    }
    if (has(upper, "FAILSAFE") || has(upper, "RADIO") || has(upper, "RC ") ||
        has(upper, "THROTTLE FS") || has(upper, "GCS FS")) {
        cat |= CAT_RC;
    }
    if (has(upper, "VIB")) {
        cat |= CAT_VIBE;
    }
    if (has(upper, "GYRO") || has(upper, "ACCEL") || has(upper, "IMU") ||
        has(upper, "INS ")) {
        cat |= CAT_INS;
    }
    // "CALIBRAT" (not "CAL") to catch calibrate/calibrated/calibrating/calibration
    // without false-matching VERTICAL / CRITICAL / LOCAL / SCALE etc.
    if (has(upper, "CALIBRAT")) {
        cat |= CAT_CAL;
    }
    // arm/disarm events, but PreArm is deliberately excluded (handled above)
    if (has(upper, "ARMING") || has(upper, "DISARM") || has(upper, "ARMED")) {
        cat |= CAT_ARM;
    }
    return cat;
}

bool should_show(uint8_t sev, uint16_t cat, uint32_t allow_mask,
                 uint8_t msg_level, uint8_t crit_override)
{
    // safety net: severe messages are always shown, regardless of filters
    if (sev <= crit_override) {
        return true;
    }
    // per-screen severity level filter (MSG_LVL): hide less-severe messages
    if (sev > msg_level) {
        return false;
    }
    // category allow-list (MSG_CAT): 0 disables it; otherwise show only messages
    // that match at least one enabled category
    if (allow_mask == 0) {
        return true;
    }
    return (cat & allow_mask) != 0;
}

Style style_for(uint8_t sev)
{
    Style s {};   // no emphasis by default
    if (sev <= SEV_CRITICAL) {
        // emergency / alert / critical: flash to demand attention
        s.blink = true;
    } else if (sev <= SEV_WARNING) {
        // error / warning: a static inverted block, prominent but not flashing
        s.invert = true;
    }
    // s.page stays 0: HD colour is a future opt-in (needs a colour font on the
    // goggles), and defaulting a non-zero page would garble text on HD systems
    // without one. The field is the extension point for that.
    return s;
}

void str_replace(char *buf, const char *from, const char *to)
{
    const size_t flen = strlen(from);
    const size_t tlen = strlen(to);
    if (flen == 0 || tlen > flen) {
        return;   // shorthand is shorten-only, so it can never overflow buf
    }
    char *p = buf;
    while ((p = strstr(p, from)) != nullptr) {
        memmove(p + tlen, p + flen, strlen(p + flen) + 1);  // shift tail incl NUL
        memcpy(p, to, tlen);
        p += tlen;   // continue past the replacement so `to` is not re-matched
    }
}

void abbreviate(char *buf)
{
    // Keys are UPPERCASE (caller upper-cases the message); most-specific phrases
    // first; every replacement is shorter than its key.
    static const struct { const char *from; const char *to; } table[] = {
        { "DISARMING MOTORS", "DISARMED" },
        { "ARMING MOTORS",    "ARMED" },
        { "PREARM: ",         "PA:" },
        { "PREARM:",          "PA:" },
        { "INTERNAL ERROR",   "INT ERR" },
        { "NOT HEALTHY",      "BAD" },
        { "UNHEALTHY",        "BAD" },
        { "INCONSISTENT",     "INCONS" },
        { "CALIBRATION",      "CAL" },
        { "CALIBRATED",       "CAL" },
        { "CALIBRATE",        "CAL" },
        { "LANE SWITCH",      "LANE" },
        { "THROTTLE",         "THR" },
        { "BATTERY",          "BAT" },
        { "VOLTAGE",          "VOLT" },
        { "VIBRATION",        "VIBE" },
        { "COMPASS",          "MAG" },
        { "POSITION",         "POS" },
        { "ALTITUDE",         "ALT" },
        { "AIRSPEED",         "ASPD" },
        { "SATELLITES",       "SATS" },
        { "FAILSAFE",         "FS" },
        { "WAITING",          "WAIT" },
        { "TERRAIN",          "TERR" },
        { "GLITCH",           "GLTCH" },
        { "ACCELS",           "ACC" },
        { "GYROS",            "GYRO" },
    };
    for (uint8_t i = 0; i < sizeof(table)/sizeof(table[0]); i++) {
        str_replace(buf, table[i].from, table[i].to);
    }
}

} // namespace AP_OSD_Msg

#endif // OSD_ENABLED
