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

#include "AP_VideoTX.h"

#if AP_VIDEOTX_ENABLED

#include <AP_RCTelemetry/AP_CRSF_Telem.h>
#include <GCS_MAVLink/GCS.h>

#include <AP_HAL/AP_HAL.h>

extern const AP_HAL::HAL& hal;

AP_VideoTX *AP_VideoTX::singleton;

const AP_Param::GroupInfo AP_VideoTX::var_info[] = {

    // @Param: ENABLE
    // @DisplayName: Is the Video Transmitter enabled or not
    // @Description: Toggles the Video Transmitter on and off
    // @Values: 0:Disable,1:Enable
    AP_GROUPINFO_FLAGS("ENABLE", 1, AP_VideoTX, _enabled, 0, AP_PARAM_FLAG_ENABLE),

    // @Param: POWER
    // @DisplayName: Video Transmitter Power Level
    // @Description: Video Transmitter Power Level. Different VTXs support different power levels, the power level chosen will be rounded down to the nearest supported power level
    // @Range: 1 1000
    AP_GROUPINFO("POWER",    2, AP_VideoTX, _power_mw, 0),

    // @Param: CHANNEL
    // @DisplayName: Video Transmitter Channel
    // @Description: Video Transmitter Channel
    // @User: Standard
    // @Range: 0 7
    AP_GROUPINFO("CHANNEL",  3, AP_VideoTX, _channel, 0),

    // @Param: BAND
    // @DisplayName: Video Transmitter Band
    // @Description: Video Transmitter Band
    // @User: Standard
    // @Values: 0:Band A,1:Band B,2:Band E,3:Airwave,4:RaceBand,5:Low RaceBand,6:1G3 Band A,7:1G3 Band B,8:Band X,9:3G3 Band A,10:3G3 Band B
    AP_GROUPINFO("BAND",  4, AP_VideoTX, _band, 0),

    // @Param: FREQ
    // @DisplayName: Video Transmitter Frequency
    // @Description: Video Transmitter Frequency. The frequency is derived from the setting of BAND and CHANNEL
    // @User: Standard
    // @ReadOnly: True
    // @Range: 1000 6000
    AP_GROUPINFO("FREQ",  5, AP_VideoTX, _frequency_mhz, 0),

    // @Param: OPTIONS
    // @DisplayName: Video Transmitter Options
    // @Description: Video Transmitter Options. Pitmode puts the VTX in a low power state. Unlocked enables certain restricted frequencies and power levels. Do not enable the Unlocked option unless you have appropriate permissions in your jurisdiction to transmit at high power levels. One stop-bit may be required for VTXs that erroneously mimic iNav behaviour.
    // @User: Advanced
    // @Bitmask: 0:Pitmode,1:Pitmode until armed,2:Pitmode when disarmed,3:Unlocked,4:Add leading zero byte to requests,5:Use 1 stop-bit in SmartAudio,6:Ignore CRC in SmartAudio,7:Ignore status updates in CRSF and blindly set VTX options
    AP_GROUPINFO("OPTIONS",  6, AP_VideoTX, _options, 0),

    // @Param: MAX_POWER
    // @DisplayName: Video Transmitter Max Power Level
    // @Description: Video Transmitter Maximum Power Level. Different VTXs support different power levels, this prevents the power aux switch from requesting too high a power level. The switch supports 6 power levels and the selected power will be a subdivision between 0 and this setting.
    // @Range: 25 1000
    AP_GROUPINFO("MAX_POWER", 7, AP_VideoTX, _max_power_mw, 800),

    // @Param: TYPES
    // @DisplayName: Allowed VTX control transports
    // @Description: Bitmask of the control transports permitted to manage the VTX. AP_VideoTX represents a single VTX, so when more than one transport is present (for example a CRSF VTX on the receiver and an MSP VTX on the goggles) this selects which one owns it. Clear a transport's bit to stop it taking control of the VTX.
    // @Bitmask: 0:CRSF,1:SmartAudio,2:Tramp,3:MSP
    // @User: Advanced
    AP_GROUPINFO("TYPES", 8, AP_VideoTX, _types_allowed, uint8_t(CRSF | SmartAudio | Tramp | MSP)),

    AP_GROUPEND
};

//#define VTX_DEBUG
#ifdef VTX_DEBUG
# define debug(fmt, args...)	hal.console->printf("VTX: " fmt "\n", ##args)
#else
# define debug(fmt, args...)	do {} while(0)
#endif

extern const AP_HAL::HAL& hal;

// the historical hardcoded band/frequency grid and band-name list now live in
// AP_VideoTX_Table (as the seeded defaults); band/channel -> frequency resolves
// through the active table (see get_frequency_mhz / get_band_and_channel below).

// mapping of power level to milliwatt to dbm
// valid power levels from SmartAudio spec, the adjacent levels might be the actual values
// so these are marked as level + 0x10 and will be switched if a dbm message proves it
AP_VideoTX::PowerLevel AP_VideoTX::_power_levels[VTX_MAX_POWER_LEVELS] = {
    // level, mw, dbm, dac
    { 0xFF,  0,    0, 0    }, // only in SA 2.1
    { 0,    25,   14, 7    },
    { 0x11, 100,  20, 0xFF }, // only in SA 2.1
    { 1,    200,  23, 16   },
    { 0x12, 400,  26, 0xFF }, // only in SA 2.1
    { 2,    500,  27, 25   },
    { 0x12, 600,  28, 0xFF }, // Tramp lies above power levels and always returns 25/100/200/400/600
    { 3,    800,  29, 40   },
    { 0x13, 1000, 30, 0xFF }, // only in SA 2.1
    { 0xFF, 0,    0,  0XFF, PowerActive::Inactive }  // slot reserved for a custom power level
};

AP_VideoTX::AP_VideoTX()
{
    if (singleton) {
        AP_HAL::panic("Too many VTXs");
        return;
    }
    singleton = this;

    AP_Param::setup_object_defaults(this, var_info);

    // seed the band/frequency + power table with the historical defaults so
    // band/channel lookups work before (and if no) a stored table is loaded
    _table.load_defaults();
}

AP_VideoTX::~AP_VideoTX(void)
{
    singleton = nullptr;
}

bool AP_VideoTX::init(void)
{
    if (_initialized) {
        return false;
    }

    // PARAMETER_CONVERSION - Added: Sept-2022
    _options.convert_parameter_width(AP_PARAM_INT16);

    // load the stored band/frequency + power table, or persist the seeded
    // defaults on first boot (falls back to RAM defaults on small-storage boards)
    _table.init();
    // seed the selectable power set from the table before resolving _power_mw
    load_power_levels_from_table();

    // find the index into the power table
    for (uint8_t i = 0; i < VTX_MAX_POWER_LEVELS; i++) {
        if (_power_mw <= _power_levels[i].mw) {
            if (_power_mw != _power_levels[i].mw) {
                if (i > 0) {
                    _current_power = i - 1;
                }
                _power_mw.set_and_save(get_power_mw());
            } else {
                _current_power = i;
            }
            break;
        }
    }
    _current_band = _band;
    _current_channel = _channel;
    _current_frequency = _frequency_mhz;
    _current_options = _options;
    _current_enabled = _enabled;
    _initialized = true;

    return true;
}

// band/channel (zero-based) -> frequency in MHz via the active table
uint16_t AP_VideoTX::get_frequency_mhz(uint8_t band, uint8_t channel)
{
    if (singleton == nullptr) {
        return 0;
    }
    return singleton->_table.frequency(band, channel);
}

bool AP_VideoTX::get_band_and_channel(uint16_t freq, VideoBand& band, uint8_t& channel)
{
    if (singleton == nullptr) {
        return false;
    }
    uint8_t b;
    if (singleton->_table.band_and_channel_for_frequency(freq, b, channel)) {
        band = VideoBand(b);
        return true;
    }
    return false;
}

// set the current power
void AP_VideoTX::set_configured_power_mw(uint16_t power)
{
    _power_mw.set_and_save_ifchanged(power);
}

uint8_t AP_VideoTX::find_current_power() const
{
    for (uint8_t i = 0; i < VTX_MAX_POWER_LEVELS; i++) {
        if (_power_mw == _power_levels[i].mw) {
            return i;
        }
    }
    return 0;
}

// SmartAudio v1 DAC value for the standard mW levels, 0xFF (unknown) otherwise
static uint8_t dac_for_mw(uint16_t mw)
{
    switch (mw) {
    case 25:  return 7;
    case 200: return 16;
    case 500: return 25;
    case 800: return 40;
    default:  return 0xFF;
    }
}

// rebuild the selectable power set from the user-defined power table so the
// existing per-protocol send paths (Tramp mW, SmartAudio level/dbm/dac) and the
// 6-position switch all operate over the table's levels. The table "value" is
// treated as mW (Tramp-native); dbm/dac are derived for SmartAudio.
void AP_VideoTX::load_power_levels_from_table()
{
    const uint8_t n = _table.num_power_levels();
    if (n == 0) {
        return;  // no user power table: keep the compiled learned defaults
    }
    for (uint8_t i = 0; i < VTX_MAX_POWER_LEVELS; i++) {
        if (i < n) {
            const uint16_t mw = _table.power_value(i);
            _power_levels[i].mw = mw;
            _power_levels[i].dbm = mw > 0 ? uint8_t(roundf(10.0f * log10f(float(mw)))) : 0;
            _power_levels[i].dac = dac_for_mw(mw);
            _power_levels[i].level = i;
            _power_levels[i].active = mw > 0 ? PowerActive::Active : PowerActive::Inactive;
        } else {
            _power_levels[i].active = PowerActive::Inactive;
        }
    }
}

void AP_VideoTX::on_table_updated()
{
    // the band/frequency side reads the table live, only the cached power set
    // needs rebuilding when the table is replaced at runtime (via @VTX FTP)
    load_power_levels_from_table();
}

void AP_VideoTX::get_power_label(uint8_t index, char *out, size_t out_len) const
{
    // index is one-based over the selectable levels, matching get_*_for_index:
    // resolve through the same non-zero slot mapping so the label and the mW
    // for a given index always refer to the same table entry
    uint8_t slot;
    if (table_power_slot(index, slot)) {
        _table.power_label(slot, out, out_len);
        return;
    }
    if (out_len > 0) {
        out[0] = 0;
    }
}

// set the power in dbm, rounding appropriately
void AP_VideoTX::set_power_dbm(uint8_t power, PowerActive active)
{
    if (power == _power_levels[_current_power].dbm
        && _power_levels[_current_power].active == active) {
        return;
    }

    for (uint8_t i = 0; i < VTX_MAX_POWER_LEVELS; i++) {
        if (power == _power_levels[i].dbm) {
            _current_power = i;
            _power_levels[i].active = active;
            debug("learned power %ddbm", power);
            // now unlearn the "other" power level since we have no other way of guessing
            // the supported levels
            if ((_power_levels[i].level & 0xF0) == 0x10) {
                _power_levels[i].level = _power_levels[i].level & 0xF;
            }
            if (i > 0 && _power_levels[i-1].level == _power_levels[i].level) {
                debug("invalidated power %dwm, level %d is now %dmw", _power_levels[i-1].mw, _power_levels[i].level, _power_levels[i].mw);
                _power_levels[i-1].level = 0xFF;
                _power_levels[i-1].active = PowerActive::Inactive;
            } else if (i < VTX_MAX_POWER_LEVELS-1 && _power_levels[i+1].level == _power_levels[i].level) {
                debug("invalidated power %dwm, level %d is now %dmw", _power_levels[i+1].mw, _power_levels[i].level, _power_levels[i].mw);
                _power_levels[i+1].level = 0xFF;
                _power_levels[i+1].active = PowerActive::Inactive;
            }
            return;
        }
    }
    // learn the non-standard power
    _current_power = update_power_dbm(power, active);
}

// add an active power setting in dbm
uint8_t AP_VideoTX::update_power_dbm(uint8_t power, PowerActive active)
{
    for (uint8_t i = 0; i < VTX_MAX_POWER_LEVELS; i++) {
        if (power == _power_levels[i].dbm) {
            if (_power_levels[i].active != active) {
                _power_levels[i].active = active;
                debug("%s power %ddbm", active == PowerActive::Active ? "learned" : "invalidated", power);
            }
            return i;
        }
    }
    // handed a non-standard value, use the last slot
    _power_levels[VTX_MAX_POWER_LEVELS-1].dbm = power;
    _power_levels[VTX_MAX_POWER_LEVELS-1].level = 255;
    _power_levels[VTX_MAX_POWER_LEVELS-1].dac = 255;
    _power_levels[VTX_MAX_POWER_LEVELS-1].mw = uint16_t(roundf(powf(10, power * 0.1f)));
    _power_levels[VTX_MAX_POWER_LEVELS-1].active = active;
    debug("non-standard power %ddbm -> %dmw", power, _power_levels[VTX_MAX_POWER_LEVELS-1].mw);
    return VTX_MAX_POWER_LEVELS-1;
}

// add all active power setting in dbm
void AP_VideoTX::update_all_power_dbm(uint8_t nlevels, const uint8_t power[])
{
    for (uint8_t i = 0; i < nlevels; i++) {
        update_power_dbm(power[i], PowerActive::Active);
    }
    // invalidate the remaining ones
    for (uint8_t i = 0; i < VTX_MAX_POWER_LEVELS; i++) {
        if (_power_levels[i].active == PowerActive::Unknown) {
            _power_levels[i].active = PowerActive::Inactive;
        }
    }
}

// mark the level matching the given mW as supported, stashing a non-standard
// value in the custom slot; does not change the currently selected power
void AP_VideoTX::update_power_mw(uint16_t power_mw, PowerActive active)
{
    for (uint8_t i = 0; i < VTX_MAX_POWER_LEVELS; i++) {
        if (power_mw == _power_levels[i].mw) {
            _power_levels[i].active = active;
            return;
        }
    }
    if (power_mw == 0) {
        return;
    }
    // non-standard value: use the custom slot
    PowerLevel &slot = _power_levels[VTX_MAX_POWER_LEVELS - 1];
    slot.mw = power_mw;
    slot.dbm = uint8_t(roundf(10.0f * log10f(float(power_mw))));
    slot.level = 255;
    slot.dac = 255;
    slot.active = active;
}

// whether the user @VTX table is the authoritative source of the selectable
// power set. On this fork init() always seeds a default table, so this is
// normally true; it only falls back to the learned ladder if the table somehow
// defines no usable (non-zero) power level.
bool AP_VideoTX::have_power_table() const
{
    for (uint8_t i = 0; i < _table.num_power_levels(); i++) {
        if (_table.power_value(i) != 0) {
            return true;
        }
    }
    return false;
}

// map a one based selectable index to a @VTX table slot, skipping unused
// (0 mW) entries. This keeps every power-index accessor consistent with the
// documented contract and the configurator: index i == the i-th NON-ZERO power
// level in the order the table lists them.
bool AP_VideoTX::table_power_slot(uint8_t index, uint8_t &slot) const
{
    uint8_t count = 0;
    const uint8_t n = _table.num_power_levels();
    for (uint8_t i = 0; i < n; i++) {
        if (_table.power_value(i) == 0) {
            continue;
        }
        if (++count == index) {
            slot = i;
            return true;
        }
    }
    return false;
}

// number of supported (selectable) power levels. Resolved against the ordered
// @VTX table when one is defined, so it is deterministic and matches the
// configurator; only when no table power levels exist does it reflect the
// runtime-learned _power_levels[] ladder.
uint8_t AP_VideoTX::get_num_power_levels() const
{
    if (have_power_table()) {
        uint8_t count = 0;
        for (uint8_t i = 0; i < _table.num_power_levels(); i++) {
            if (_table.power_value(i) != 0) {
                count++;
            }
        }
        return count;
    }
    uint8_t count = 0;
    for (uint8_t i = 0; i < VTX_MAX_POWER_LEVELS; i++) {
        if (_power_levels[i].active == PowerActive::Active) {
            count++;
        }
    }
    return count;
}

// mW for a one based index into the supported levels, 0 if unknown. Table
// order when a user table is defined (index i -> i-th non-zero table entry),
// otherwise the learned ladder.
uint16_t AP_VideoTX::get_power_mw_for_index(uint8_t index) const
{
    if (have_power_table()) {
        uint8_t slot;
        return table_power_slot(index, slot) ? _table.power_value(slot) : 0;
    }
    uint8_t count = 0;
    for (uint8_t i = 0; i < VTX_MAX_POWER_LEVELS; i++) {
        if (_power_levels[i].active == PowerActive::Active && ++count == index) {
            return _power_levels[i].mw;
        }
    }
    return 0;
}

// one based index into the supported levels for a mW value, 0 if not matched.
// Table order when a user table is defined, otherwise the learned ladder.
uint8_t AP_VideoTX::get_power_index_for_mw(uint16_t power_mw) const
{
    if (have_power_table()) {
        uint8_t count = 0;
        for (uint8_t i = 0; i < _table.num_power_levels(); i++) {
            const uint16_t v = _table.power_value(i);
            if (v == 0) {
                continue;
            }
            count++;
            if (v == power_mw) {
                return count;
            }
        }
        return 0;
    }
    uint8_t count = 0;
    for (uint8_t i = 0; i < VTX_MAX_POWER_LEVELS; i++) {
        if (_power_levels[i].active == PowerActive::Active) {
            count++;
            if (_power_levels[i].mw == power_mw) {
                return count;
            }
        }
    }
    return 0;
}

// set the power in mw
void AP_VideoTX::set_power_mw(uint16_t power)
{
    for (uint8_t i = 0; i < VTX_MAX_POWER_LEVELS; i++) {
        if (power == _power_levels[i].mw) {
            _current_power = i;
            return;
        }
    }
    // Non-standard value (e.g. Tramp 2500 mW). Populate the custom slot
    // so get_power_mw() reflects what the device reported and the
    // update_power() equality short-circuit can fire on subsequent calls.
    PowerLevel &slot = _power_levels[VTX_MAX_POWER_LEVELS - 1];
    slot.mw = power;
    slot.dbm = uint8_t(roundf(10.0f * log10f(float(power))));
    slot.level = 255;
    slot.dac = 255;
    slot.active = PowerActive::Active;
    _current_power = VTX_MAX_POWER_LEVELS - 1;
}

// set the power "level"
void AP_VideoTX::set_power_level(uint8_t level, PowerActive active)
{
    if (level == _power_levels[_current_power].level
        && _power_levels[_current_power].active == active) {
        return;
    }

    for (uint8_t i = 0; i < VTX_MAX_POWER_LEVELS; i++) {
        if (level == _power_levels[i].level) {
            _current_power = i;
            _power_levels[i].active = active;
            debug("learned power level %d: %dmw", level, get_power_mw());
            break;
        }
    }
}

// set the power dac
void AP_VideoTX::set_power_dac(uint16_t power, PowerActive active)
{
    if (power == _power_levels[_current_power].dac
        && _power_levels[_current_power].active == active) {
        return;
    }

    for (uint8_t i = 0; i < VTX_MAX_POWER_LEVELS; i++) {
        if (power == _power_levels[i].dac) {
            _current_power = i;
            _power_levels[i].active = active;
            debug("learned power %dmw", get_power_mw());
        }
    }
}

// set the current channel
void AP_VideoTX::set_enabled(bool enabled)
{
    _current_enabled = enabled;
    if (!_enabled.configured()) {
        _enabled.set_and_save(enabled);
    }
}

void AP_VideoTX::set_power_is_current()
{
    set_power_dbm(get_configured_power_dbm());
}

void AP_VideoTX::set_freq_is_current()
{
    _current_frequency = _frequency_mhz;
    _current_band = _band;
    _current_channel = _channel;
}

// periodic update
void AP_VideoTX::update(void)
{
    if (!_enabled) {
        return;
    }

    // manipulate pitmode if pitmode-on-disarm or power-on-arm is set
    if (has_option(VideoOptions::VTX_PITMODE_ON_DISARM) || has_option(VideoOptions::VTX_PITMODE_UNTIL_ARM)) {
        if (hal.util->get_soft_armed() && has_option(VideoOptions::VTX_PITMODE)) {
            _options.set(_options & ~uint8_t(VideoOptions::VTX_PITMODE));
        } else if (!hal.util->get_soft_armed() && !has_option(VideoOptions::VTX_PITMODE)
            && has_option(VideoOptions::VTX_PITMODE_ON_DISARM)) {
            _options.set(_options | uint8_t(VideoOptions::VTX_PITMODE));
        }
    }
    // check that the requested power is actually allowed
    // reset if not
    if (_power_mw != get_power_mw()) {
        if (_power_levels[find_current_power()].active == PowerActive::Inactive) {
            // reset to something we know works
            debug("power reset to %dmw from %dmw", get_power_mw(), _power_mw.get());
            _power_mw.set_and_save(get_power_mw());
        }
    }
}

bool AP_VideoTX::update_options() const
{
    if (!_defaults_set) {
        return false;
    }
    // check pitmode
    if ((_options & uint8_t(VideoOptions::VTX_PITMODE))
        != (_current_options & uint8_t(VideoOptions::VTX_PITMODE))) {
        return true;
    }

#if HAL_CRSF_TELEM_ENABLED
    // using CRSF so unlock is not an option
    if (AP::crsf_telem() != nullptr) {
        return false;
    }
#endif
    // check unlock only
    if ((_options & uint8_t(VideoOptions::VTX_UNLOCKED)) != 0
        && (_current_options & uint8_t(VideoOptions::VTX_UNLOCKED)) == 0) {
        return true;
    }

    // ignore everything else
    return false;
}

bool AP_VideoTX::update_power() const {
    if (!_defaults_set || _power_mw == get_power_mw() || get_pitmode()) {
        return false;
    }
    // check that the requested power is actually allowed
    for (uint8_t i = 0; i < VTX_MAX_POWER_LEVELS; i++) {
        if (_power_mw == _power_levels[i].mw
            && _power_levels[i].active != PowerActive::Inactive) {
            return true;
        }
    }
    // Tramp's protocol carries an arbitrary 16-bit mW value with no
    // table-membership constraint, so allow non-standard values when a
    // Tramp backend is active. SmartAudio v1/v2 retain the table check;
    // SA 2.1 will have populated the table dynamically before reaching here.
    if (_power_mw > 0 && is_provider_enabled(VTXType::Tramp)) {
        return true;
    }
    // asked for something unsupported - only SA2.1 allows this and will have already provided a list
    return false;
}

bool AP_VideoTX::have_params_changed() const
{
    return _enabled
        && (update_power()
        || update_band()
        || update_channel()
        || update_frequency()
        || update_options());
}

// update the configured frequency to match the channel and band
void AP_VideoTX::update_configured_frequency()
{
    _frequency_mhz.set_and_save(get_frequency_mhz(_band, _channel));
}

// update the configured channel and band to match the frequency
void AP_VideoTX::update_configured_channel_and_band()
{
    VideoBand band;
    uint8_t channel;
    if (get_band_and_channel(_frequency_mhz, band, channel)) {
        _band.set_and_save(band);
        _channel.set_and_save(channel);
    } else {
        update_configured_frequency();
    }
}

// set the current configured values if not currently set in storage
// this is necessary so that the current settings can be seen
bool AP_VideoTX::set_defaults()
{
    if (_defaults_set) {
        return false;
    }

    // check that our current view of frequency matches band/channel
    // if not then force one to be correct
    uint16_t calced_freq = get_frequency_mhz(_current_band, _current_channel);
    if (_current_frequency != calced_freq) {
        if (_current_frequency > 0) {
            VideoBand band;
            uint8_t channel;
            if (get_band_and_channel(_current_frequency, band, channel)) {
                _current_band = band;
                _current_channel = channel;
            } else {
                _current_frequency = calced_freq;
            }
        } else {
            _current_frequency = calced_freq;
        }
    }

    if (!_options.configured()) {
        _options.set_and_save(_current_options);
    }
    if (!_channel.configured()) {
        _channel.set_and_save(_current_channel);
    }
    if (!_band.configured()) {
        _band.set_and_save(_current_band);
    }
    if (!_power_mw.configured()) {
        _power_mw.set_and_save(get_power_mw());
    }
    if (!_frequency_mhz.configured()) {
        _frequency_mhz.set_and_save(_current_frequency);
    }

    // Now check that the user didn't screw up by selecting incompatible options
    if (_frequency_mhz != get_frequency_mhz(_band, _channel)) {
        if (_frequency_mhz > 0) {
            update_configured_channel_and_band();
        } else {
            update_configured_frequency();
        }
    }

    _defaults_set = true;

    announce_vtx_settings();

    return true;
}

void AP_VideoTX::announce_vtx_settings() const
{
    // Output a friendly message so the user knows the VTX has been detected
    GCS_SEND_TEXT(MAV_SEVERITY_INFO, "VTX: %c%d %dMHz, PWR: %dmW",
        _table.band_letter(_band.get()), _channel.get() + 1, _frequency_mhz.get(),
        has_option(VideoOptions::VTX_PITMODE) ? 0 : _power_mw.get());
}

// change the video power based on switch input
// 6-pos range is in the middle of the available range
void AP_VideoTX::change_power(int8_t position)
{
    if (!_enabled || position < 0 || position > 5) {
        return;
    }
    // build the selectable set from the authoritative power levels (the ordered
    // @VTX table when one is defined, otherwise the learned ladder) via the
    // shared accessors, keeping only levels within the configured maximum. This
    // keeps the low/mid/high position path consistent with the exact-index path
    // and the OSD, all resolving against the same source of truth.
    const uint8_t n = get_num_power_levels();
    uint16_t levels[VTX_MAX_POWER_LEVELS];
    uint8_t num_active_levels = 0;
    for (uint8_t i = 1; i <= n && num_active_levels < VTX_MAX_POWER_LEVELS; i++) {
        const uint16_t mw = get_power_mw_for_index(i);
        if (mw != 0 && mw <= _max_power_mw) {
            levels[num_active_levels++] = mw;
        }
    }

    uint16_t power = 0;
    if (num_active_levels > 0) {
        // map the 6 switch positions proportionally across the available levels
        const uint16_t level = constrain_int16(roundf((num_active_levels * (position + 1) / 6.0f) - 1), 0, num_active_levels - 1);
        power = levels[level];
        debug("pos %d -> level %d/%d = %dmw", position, level, num_active_levels, power);
    }

    if (position == 5 && power < _max_power_mw) {
        power = _max_power_mw;
        debug("selected power %dmw", power);
    }

    if (power == 0) {
        if (!hal.util->get_soft_armed()) {    // don't allow pitmode to be entered if already armed
            set_configured_options(get_configured_options() | uint8_t(VideoOptions::VTX_PITMODE));
        }
    } else {
        if (has_option(VideoOptions::VTX_PITMODE)) {
            set_configured_options(get_configured_options() & ~uint8_t(VideoOptions::VTX_PITMODE));
        }
        set_configured_power_mw(power);
    }
}

namespace AP {
    AP_VideoTX& vtx() {
        return *AP_VideoTX::get_singleton();
    }
};

#endif
