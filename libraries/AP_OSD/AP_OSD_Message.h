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
/*
  Pure, backend-independent helpers for the OSD MESSAGE panel: classify a
  message into subsystem categories, decide whether a screen should show it
  (severity level + category allow-list, with a safety override), pick blink
  emphasis by severity, and apply the built-in abbreviation dictionary. Kept
  free of HAL/param/singleton dependencies so it is unit-testable on its own
  (see libraries/AP_OSD/tests/test_osd_message.cpp).
*/
#pragma once

#include "AP_OSD_config.h"

#if OSD_ENABLED

#include <stdint.h>
#include <stddef.h>

namespace AP_OSD_Msg {

// Subsystem categories, matched by keyword against the (upper-cased) message
// text. A message may belong to several at once. Exposed to the user as the
// OSD_MSG_CAT allow-list bitmask, so the bit positions are part of the API -
// only ever append.
enum Category : uint16_t {
    CAT_PREARM  = 1U << 0,   // "PreArm: ..."
    CAT_EKF     = 1U << 1,   // EKF / AHRS
    CAT_GPS     = 1U << 2,   // GPS / GNSS
    CAT_BATT    = 1U << 3,   // battery / voltage / power / current
    CAT_COMPASS = 1U << 4,   // compass / mag
    CAT_RC      = 1U << 5,   // RC / radio / failsafe
    CAT_VIBE    = 1U << 6,   // vibration
    CAT_INS     = 1U << 7,   // gyro / accel / IMU
    CAT_CAL     = 1U << 8,   // calibration
    CAT_ARM     = 1U << 9,   // arm / disarm events (not PreArm)
};

// MAV_SEVERITY is lower-is-more-severe. Duplicated here as bare values so this
// module needs no MAVLink dependency; they match the MAV_SEVERITY enum.
static const uint8_t SEV_EMERGENCY = 0;
static const uint8_t SEV_ALERT     = 1;
static const uint8_t SEV_CRITICAL  = 2;
static const uint8_t SEV_ERROR     = 3;
static const uint8_t SEV_WARNING   = 4;
static const uint8_t SEV_NOTICE    = 5;
static const uint8_t SEV_INFO      = 6;
static const uint8_t SEV_DEBUG     = 7;

// classify an already-UPPER-CASED message into a Category bitmask (0 = none).
// Must be called on the raw text, before abbreviation shortens keywords.
uint16_t classify(const char *upper);

// Decide whether a MESSAGE panel should display this message.
//   sev          : MAV_SEVERITY of the message (lower = more severe)
//   cat          : classify() result for the message
//   allow_mask   : OSD_MSG_CAT allow-list; 0 disables category filtering
//   msg_level    : per-screen least-severe level to show (MSG_LVL)
//   crit_override: messages at this severity or more severe are always shown,
//                  bypassing the category allow-list (safety net)
bool should_show(uint8_t sev, uint16_t cat, uint32_t allow_mask,
                 uint8_t msg_level, uint8_t crit_override);

// How a MESSAGE-panel line should be emphasised. Backend-agnostic: each OSD
// backend applies the attributes it supports (analog MAX7456: blink + invert;
// HD MSP DisplayPort: blink + colour font page) and ignores the rest.
struct Style {
    bool blink;     // flash the text (most-severe messages)
    bool invert;    // draw inverted (black-on-white) - an analog attention block
    uint8_t page;   // HD colour font page (0 = default); reserved for HD colour
};

// Map a message severity to its display style. The single place the
// severity -> emphasis policy lives.
Style style_for(uint8_t sev);

// replace every occurrence of `from` with `to` in buf, in place. Only shortens
// (a `to` longer than `from` is ignored) so buf can never grow or overflow.
void str_replace(char *buf, const char *from, const char *to);

// apply the built-in abbreviation dictionary to buf in place (shorten-only).
void abbreviate(char *buf);

} // namespace AP_OSD_Msg

#endif // OSD_ENABLED
