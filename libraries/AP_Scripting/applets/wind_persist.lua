--[[
  Wind Persistence Lua Applet

  Learns the EKF horizontal wind estimate while the vehicle is flying with
  GPS, saves the settled value to parameters that survive a reboot, and
  reinjects it into the EKF on the next GPS-denied arm so dead reckoning
  starts with a realistic wind state.

  Extracted from cruise_mode.lua so the wind learn/save/warm-start
  mechanism can be used on its own, independent of any flight mode.

  Operation:
    - While armed, flying, and the EKF is fusing GPS, the script samples
      ahrs:wind_estimate() at 1 Hz, waits for the estimate to settle
      (low speed and direction variance over a short window), then saves
      it to WIND_SPD/DIR/SACC/DACC and the location WIND_LAT/LON.
    - On the disarmed->armed transition, if the EKF is NOT fusing GPS
      (a GPS-denied takeoff) and a value has been saved, it pushes the
      saved wind back to the EKF via ahrs:handle_external_wind_estimate.
      Skipped if the current location is further than WIND_DIST from the
      saved location, so a stale value is not used after a long transit.

  Requires the ahrs:using_gps() and ahrs:handle_external_wind_estimate()
  bindings.
]]

local MAV_SEVERITY = {
    EMERGENCY = 0,
    ALERT = 1,
    CRITICAL = 2,
    ERROR = 3,
    WARNING = 4,
    NOTICE = 5,
    INFO = 6,
    DEBUG = 7
}

local PARAM_TABLE_KEY = 103 -- must be unique among scripts on this flight controller
local PARAM_TABLE_PREFIX = "WIND_"
assert(param:add_table(PARAM_TABLE_KEY, PARAM_TABLE_PREFIX, 8), "Could not add param table")

local function bind_param(name)
    local p = Parameter()
    assert(p:init(name), string.format('could not find %s parameter', name))
    return p
end

local function bind_add_param(name, idx, default_value)
    assert(param:add_param(PARAM_TABLE_KEY, idx, name, default_value), string.format('could not add param %s', name))
    return bind_param(PARAM_TABLE_PREFIX .. name)
end

--[[
  // @Param: WIND_OPT
  // @DisplayName: Wind Persistence Enable
  // @Description: Master enable for the wind persistence applet. When 0 the script does nothing. When 1 it learns and saves the EKF wind while flying with GPS and reinjects it on the next GPS-denied arm.
  // @User: Standard
  // @Values: 0:Disabled,1:Enabled
--]]
local WIND_OPT = bind_add_param("OPT", 1, 1)

--[[
  // @Param: WIND_SPD
  // @DisplayName: Persisted Wind Speed
  // @Description: Persisted horizontal wind speed captured while flying with GPS being fused. Used to warm-start the EKF on a subsequent GPS-denied flight. Set automatically by the script; 0 means no value has been recorded.
  // @User: Standard
  // @Units: m/s
  // @Range: 0 50
--]]
local WIND_SPD = bind_add_param("SPD", 2, 0)

--[[
  // @Param: WIND_DIR
  // @DisplayName: Persisted Wind Direction
  // @Description: Persisted azimuth (deg from true north) the wind is coming from. Set automatically by the script alongside WIND_SPD.
  // @User: Standard
  // @Units: deg
  // @Range: 0 360
--]]
local WIND_DIR = bind_add_param("DIR", 3, 0)

--[[
  // @Param: WIND_SACC
  // @DisplayName: Persisted Wind Speed Accuracy
  // @Description: Persisted 1-sigma uncertainty of WIND_SPD in m/s. Set automatically by the script from the in-flight wind variability.
  // @User: Advanced
  // @Units: m/s
  // @Range: 0 10
--]]
local WIND_SACC = bind_add_param("SACC", 4, 0)

--[[
  // @Param: WIND_DACC
  // @DisplayName: Persisted Wind Direction Accuracy
  // @Description: Persisted 1-sigma uncertainty of WIND_DIR in deg. Set automatically by the script from the in-flight wind variability.
  // @User: Advanced
  // @Units: deg
  // @Range: 0 90
--]]
local WIND_DACC = bind_add_param("DACC", 5, 0)

--[[
  // @Param: WIND_LAT
  // @DisplayName: Persisted Wind Location Latitude
  // @Description: Latitude (deg) at which the persisted wind was last recorded. Used to skip the warm-start if the vehicle has been moved more than WIND_DIST since.
  // @User: Standard
  // @Units: deg
--]]
local WIND_LAT = bind_add_param("LAT", 6, 0)

--[[
  // @Param: WIND_LON
  // @DisplayName: Persisted Wind Location Longitude
  // @Description: Longitude (deg) at which the persisted wind was last recorded. Used together with WIND_LAT for the staleness check.
  // @User: Standard
  // @Units: deg
--]]
local WIND_LON = bind_add_param("LON", 7, 0)

--[[
  // @Param: WIND_DIST
  // @DisplayName: Wind Warm-Start Distance Threshold
  // @Description: If the current location is further than this distance from WIND_LAT/LON, the persisted wind is considered stale and the warm-start is skipped. Set to 0 to disable the distance check.
  // @User: Advanced
  // @Units: m
  // @Range: 0 200000
--]]
local WIND_DIST = bind_add_param("DIST", 8, 50000)

-- Settling thresholds and timing for the wind learn/save logic
local WIND_SAMPLE_PERIOD_MS = 1000
local WIND_HISTORY_LEN      = 5      -- 5 s window at 1 Hz
local WIND_FIRST_SAVE_MS    = 10000  -- earliest save after learning starts
local WIND_REPEAT_SAVE_MS   = 30000  -- subsequent saves: 30 s
local WIND_SETTLED_SPD_STD  = 0.5    -- m/s
local WIND_SETTLED_DIR_STD  = 5.0    -- deg
local WIND_DELTA_SPD        = 0.5    -- m/s, threshold to trigger repeat save
local WIND_DELTA_DIR        = 5.0    -- deg

local g_state = {
    was_armed = false,         -- arming state observed last loop
    learn_start_ms = 0,        -- millis() when learning conditions became continuously true
    last_wind_sample_ms = 0,   -- last 1 Hz sample of EKF wind
    wind_history = {},         -- last N samples: {{spd=, dir=}, ...}
    last_wind_save_ms = 0,     -- last set_and_save() of wind params (this session)
}

-- Helper: shortest-arc difference in degrees, result in -180..180
local function wrap_180_deg(diff)
    while diff > 180 do diff = diff - 360 end
    while diff < -180 do diff = diff + 360 end
    return diff
end

-- Helper: true when the active EKF source set is configured to use GPS.
-- ahrs:using_gps() is a source-set configuration check, not the
-- filter-status fusion flag, so it does not flicker on individual
-- position observations.
local function ekf_using_gps()
    return ahrs:using_gps()
end

-- Helper: read EKF wind estimate as (speed_m_s, dir_from_deg_north).
-- ahrs:wind_estimate() is NED in m/s and points the way the wind is going.
local function get_wind_speed_dir()
    local wind = ahrs:wind_estimate()
    local wx, wy = wind:x(), wind:y()
    local spd = math.sqrt(wx * wx + wy * wy)
    local dir = math.deg(math.atan(-wy, -wx))
    if dir < 0 then dir = dir + 360 end
    return spd, dir
end

-- Helper: mean and stddev of speed samples
local function speed_stats(samples)
    local sum = 0
    for _, s in ipairs(samples) do sum = sum + s.spd end
    local mean = sum / #samples
    local sumsq = 0
    for _, s in ipairs(samples) do
        local d = s.spd - mean
        sumsq = sumsq + d * d
    end
    return mean, math.sqrt(sumsq / #samples)
end

-- Helper: circular mean of "wind from" angles in degrees
local function dir_circular_mean(samples)
    local sx, sy = 0, 0
    for _, s in ipairs(samples) do
        sx = sx + math.cos(math.rad(s.dir))
        sy = sy + math.sin(math.rad(s.dir))
    end
    local mean = math.deg(math.atan(sy, sx))
    if mean < 0 then mean = mean + 360 end
    return mean
end

-- Helper: stddev of direction samples around a circular mean
local function dir_stddev(samples, mean)
    local sumsq = 0
    for _, s in ipairs(samples) do
        local d = wrap_180_deg(s.dir - mean)
        sumsq = sumsq + d * d
    end
    return math.sqrt(sumsq / #samples)
end

-- Save persisted wind to params (set_and_save commits to flash)
local function save_wind_to_params(spd, dir, sacc, dacc, loc)
    WIND_SPD:set_and_save(spd)
    WIND_DIR:set_and_save(dir)
    WIND_SACC:set_and_save(sacc)
    WIND_DACC:set_and_save(dacc)
    if loc then
        WIND_LAT:set_and_save(loc:lat() * 1e-7)
        WIND_LON:set_and_save(loc:lng() * 1e-7)
    end
end

-- Push the persisted wind to the EKF. Returns true on push.
local function push_warmstart()
    local saved_spd = WIND_SPD:get()
    if saved_spd <= 0 then return false end
    local saved_dir = WIND_DIR:get()
    local saved_sacc = WIND_SACC:get()
    local saved_dacc = WIND_DACC:get()
    -- Pass NaN for unknown accuracies so the EKF uses its internal defaults
    local sacc = saved_sacc > 0 and saved_sacc or (0/0)
    local dacc = saved_dacc > 0 and saved_dacc or (0/0)
    return ahrs:handle_external_wind_estimate(saved_spd, sacc, saved_dir, dacc)
end

-- Try to reinject the persisted wind. Skips if disabled, if the EKF is
-- already fusing GPS, if no value persisted, or if the saved location is
-- further than WIND_DIST from the current location. Returns true if a
-- push to the EKF was actually issued.
local function attempt_warmstart()
    if WIND_OPT:get() == 0 then return false end
    if ekf_using_gps() then return false end
    local saved_spd = WIND_SPD:get()
    if saved_spd <= 0 then return false end

    local thresh_m = WIND_DIST:get()
    if thresh_m > 0 then
        local saved_lat = WIND_LAT:get()
        local saved_lon = WIND_LON:get()
        local cur_loc = ahrs:get_location()
        if cur_loc and (saved_lat ~= 0 or saved_lon ~= 0) then
            local saved_loc = Location()
            saved_loc:lat(math.floor(saved_lat * 1e7 + 0.5))
            saved_loc:lng(math.floor(saved_lon * 1e7 + 0.5))
            local dist = cur_loc:get_distance(saved_loc)
            if dist > thresh_m then
                gcs:send_text(MAV_SEVERITY.WARNING,
                    string.format("Wind: persisted %.0fm away (>%.0fm), skipping warm-start",
                                  dist, thresh_m))
                return false
            end
        end
    end

    if push_warmstart() then
        gcs:send_text(MAV_SEVERITY.INFO,
            string.format("Wind: warm-started %.1fm/s from %.0fdeg",
                          WIND_SPD:get(), WIND_DIR:get()))
        return true
    end

    gcs:send_text(MAV_SEVERITY.WARNING, "Wind: warm-start rejected by EKF")
    return false
end

-- Sample EKF wind once per second and save settled values to params.
-- Only runs while armed, flying, and the EKF is fusing GPS, so the wind
-- estimate is being learned reliably from GPS-constrained ground velocity.
-- First save targets ~10 s after those conditions are first met;
-- subsequent saves are gated by WIND_REPEAT_SAVE_MS plus delta thresholds.
local function update_wind_persistence(now_ms)
    if not arming:is_armed() or not vehicle:get_likely_flying() or not ekf_using_gps() then
        g_state.learn_start_ms = 0
        return
    end

    if g_state.learn_start_ms == 0 then
        g_state.learn_start_ms = now_ms
        g_state.wind_history = {}
    end

    if (now_ms - g_state.last_wind_sample_ms):tofloat() < WIND_SAMPLE_PERIOD_MS then
        return
    end
    g_state.last_wind_sample_ms = now_ms

    local spd, dir = get_wind_speed_dir()
    table.insert(g_state.wind_history, {spd = spd, dir = dir})
    while #g_state.wind_history > WIND_HISTORY_LEN do
        table.remove(g_state.wind_history, 1)
    end

    if #g_state.wind_history < WIND_HISTORY_LEN then return end

    local mean_spd, stddev_spd = speed_stats(g_state.wind_history)
    local mean_dir = dir_circular_mean(g_state.wind_history)
    local stddev_dir = dir_stddev(g_state.wind_history, mean_dir)

    if stddev_spd >= WIND_SETTLED_SPD_STD or stddev_dir >= WIND_SETTLED_DIR_STD then
        return
    end

    local time_learning_ms = (now_ms - g_state.learn_start_ms):tofloat()

    if g_state.last_wind_save_ms == 0 then
        if time_learning_ms < WIND_FIRST_SAVE_MS then return end
    else
        local time_since_save_ms = (now_ms - g_state.last_wind_save_ms):tofloat()
        if time_since_save_ms < WIND_REPEAT_SAVE_MS then return end
        local d_spd = math.abs(mean_spd - WIND_SPD:get())
        local d_dir = math.abs(wrap_180_deg(mean_dir - WIND_DIR:get()))
        if d_spd < WIND_DELTA_SPD and d_dir < WIND_DELTA_DIR then return end
    end

    save_wind_to_params(mean_spd, mean_dir,
                        math.max(WIND_SETTLED_SPD_STD, stddev_spd),
                        math.max(WIND_SETTLED_DIR_STD * 0.5, stddev_dir),
                        ahrs:get_location())
    g_state.last_wind_save_ms = now_ms
    gcs:send_text(MAV_SEVERITY.INFO,
        string.format("Wind: saved %.1fm/s from %.0fdeg (sigma %.1f/%.0f)",
                      mean_spd, mean_dir, stddev_spd, stddev_dir))
end

local function update()
    if WIND_OPT:get() == 0 then
        return update, 1000
    end

    local now_ms = millis()

    -- Arming edge: reinject the persisted wind on a GPS-denied takeoff.
    -- attempt_warmstart() self-gates on GPS-not-in-use, saved-value-present
    -- and distance staleness.
    local is_armed = arming:is_armed()
    if is_armed and not g_state.was_armed then
        attempt_warmstart()
    end
    g_state.was_armed = is_armed

    update_wind_persistence(now_ms)

    return update, 200 -- 5 Hz
end

local function protected_update()
    local success, ret1, ret2 = pcall(update)
    if not success then
        gcs:send_text(MAV_SEVERITY.CRITICAL, "Wind: SCRIPT ERROR: " .. tostring(ret1))
        return protected_update, 5000
    end
    return ret1, ret2
end

gcs:send_text(MAV_SEVERITY.INFO, "Wind persistence applet loaded")

return protected_update, 1000
