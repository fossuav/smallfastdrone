--[[
   OSD MESSAGE-panel test injector.

   Emits a rotating set of STATUSTEXT messages spanning the OSD message
   categories (PreArm / EKF / GPS / Battery / Compass / RC / Vibe / INS / Arm)
   at a range of severities, so you can watch OSD_MSG_CAT (category allow-list),
   per-screen MSG_LVL (severity level) and OSD_MSG_STYLE (blink on severe)
   behave live in SITL.

   gcs:send_text() routes through GCS::send_text -> AP_Notify::send_text, which
   is exactly the source the OSD MESSAGE panel reads.

   Try, for example:
     OSD_MSG_CAT   = 4     (bit 2 = GPS only)  -> only GPS lines show,
                                                   except CRITICAL ones (safety net)
     OSD_MSG_STYLE = 1     -> CRITICAL-and-worse messages blink
     OSD1_MSG_LVL  = 4     -> hide INFO/NOTICE, keep WARNING and worse
--]]

-- MAV_SEVERITY
local EMERGENCY, ALERT, CRITICAL, ERROR, WARNING, NOTICE, INFO = 0, 1, 2, 3, 4, 5, 6

-- {severity, text} - text chosen so classify() tags each with a category
local MESSAGES = {
    {INFO,     "GPS 1: detected u-blox"},          -- GPS, routine
    {WARNING,  "EKF3 IMU0 stopped aiding"},        -- EKF, warning
    {CRITICAL, "PreArm: Compass not calibrated"},  -- PreArm+Compass, critical (safety net)
    {NOTICE,   "Battery 1 low voltage"},           -- Battery
    {ALERT,    "Radio failsafe"},                  -- RC, alert
    {INFO,     "Vibration compensation ON"},       -- Vibe
    {ERROR,    "Gyros inconsistent"},              -- INS
    {NOTICE,   "Arming motors"},                   -- Arm
    {INFO,     "Takeoff complete"},                -- no category (hidden if allow-list set)
}

local idx = 1

function update()
    local m = MESSAGES[idx]
    gcs:send_text(m[1], m[2])
    idx = idx + 1
    if idx > #MESSAGES then
        idx = 1
    end
    return update, 4000   -- one message every 4 s (MSG panel shows each ~msgtime)
end

gcs:send_text(INFO, "osd_msg_test: injecting OSD messages")
return update, 2000
