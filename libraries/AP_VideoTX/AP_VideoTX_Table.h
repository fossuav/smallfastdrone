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
  User-definable VTX band/frequency + power tables, modelled on Betaflight's
  vtxTable. Two decoupled tables:

    - bands: up to MAX_BANDS bands, each with a name, a single-letter id, a
      factory/custom flag and up to MAX_CHANNELS channel frequencies in MHz
      (0 = channel disabled). band+channel resolves to a frequency.

    - power levels: up to MAX_POWER_LEVELS entries, each a protocol "value"
      (what is sent to the VTX: mW for Tramp, index/dBm for SmartAudio) and a
      short display "label" ("25", "200", "1W"). Power is selected by a
      one-based index; the value/label split lets one table serve every
      protocol (see README).

  The table is held in RAM here; persistence (StorageManager blob) and the
  MAVLink FTP transport are layered on top. When no table is stored the model
  is seeded from the historical compiled-in bands so behaviour is unchanged.
*/
#pragma once

#include "AP_VideoTX_config.h"

#if AP_VIDEOTX_TABLE_ENABLED

#include <AP_HAL/AP_HAL.h>
#include <AP_Common/AP_Common.h>

class AP_VideoTX_Table {
public:
    // limits. MAX_BANDS is >= the 11 historical bands so every legacy
    // AP_VideoTX::VideoBand index maps to a real default band, with headroom
    // for user-added custom bands. Channels/power match Betaflight (8/8).
    static const uint8_t MAX_BANDS = 12;
    static const uint8_t MAX_CHANNELS = 8;
    static const uint8_t MAX_POWER_LEVELS = 8;
    static const uint8_t BAND_NAME_LEN = 8;    // not NUL terminated in storage
    static const uint8_t POWER_LABEL_LEN = 3;  // not NUL terminated in storage

    struct Band {
        char name[BAND_NAME_LEN];    // space/zero padded, not NUL terminated
        char letter;                 // single-char id shown in the OSD
        bool is_factory;             // factory band: VTX uses its own freq map
        uint16_t freq[MAX_CHANNELS]; // MHz, 0 = channel disabled/unused
    };

    struct PowerLevel {
        uint16_t value;                // protocol value (mW / index / dBm)
        char label[POWER_LABEL_LEN];   // display string, not NUL terminated
    };

    AP_VideoTX_Table() {}
    CLASS_NO_COPY(AP_VideoTX_Table);

    // wire format / storage constants
    static const uint16_t BLOB_MAGIC = 0x5654;   // 'VT'
    static const uint8_t  BLOB_VERSION = 1;
    // worst case serialized size (must fit the StorageVTXTable region)
    static const uint16_t BLOB_MAX = 6 + MAX_BANDS*(BAND_NAME_LEN+2+MAX_CHANNELS*2)
                                       + MAX_POWER_LEVELS*(2+POWER_LABEL_LEN) + 4;

    // load the table from persistent storage; if absent or invalid, seed the
    // compiled defaults and persist them. Call once at startup.
    void init();
    // persist the current table to storage, false if storage is unavailable/full
    bool save();

    // serialize the current table into buf (must be >= BLOB_MAX bytes),
    // returns the number of bytes written. Used by the @VTX FTP transport.
    uint16_t to_blob(uint8_t *buf) const { return serialize(buf); }
    // replace the table from a serialized blob and persist it; returns false
    // (leaving the table unchanged) if the blob is malformed
    bool from_blob(const uint8_t *buf, uint16_t len) {
        if (!deserialize(buf, len)) {
            return false;
        }
        save();
        return true;
    }

    // seed the model from the historical compiled-in bands + standard powers
    void load_defaults();

    // -- band / frequency accessors (band, channel are zero-based) --
    uint8_t num_bands() const { return _num_bands; }
    uint8_t num_channels() const { return _num_channels; }
    // frequency for a band/channel, 0 if out of range or channel disabled
    uint16_t frequency(uint8_t band, uint8_t channel) const;
    // reverse lookup: first band/channel whose frequency matches, false if none
    bool band_and_channel_for_frequency(uint16_t freq, uint8_t &band, uint8_t &channel) const;
    // single-letter band id, '?' if out of range
    char band_letter(uint8_t band) const;
    // copy a NUL-terminated band name into out (size >= BAND_NAME_LEN+1)
    void band_name(uint8_t band, char *out, size_t out_len) const;
    bool band_is_factory(uint8_t band) const;

    // -- power accessors (index is zero-based here) --
    uint8_t num_power_levels() const { return _num_power_levels; }
    uint16_t power_value(uint8_t index) const;
    void power_label(uint8_t index, char *out, size_t out_len) const;

    // whether a usable table is loaded
    bool is_valid() const { return _num_bands > 0 && _num_channels > 0; }

    // -- incremental setters (used by the MSP VTXTABLE ingest path; band and
    //    index are zero-based). They grow the counts to cover what is written,
    //    so callers need not pre-declare dimensions. Persist with save().
    // set/clear table dimensions and wipe existing entries
    void set_dims(uint8_t bands, uint8_t channels, uint8_t power_levels);
    // define one band (name/label are byte buffers with an explicit length,
    // not NUL terminated); false if band is out of range
    bool set_band(uint8_t band, const char *name, uint8_t name_len, char letter,
                  bool is_factory, const uint16_t *freq, uint8_t nfreq);
    // define one power level; false if index is out of range
    bool set_power_level(uint8_t index, uint16_t value, const char *label, uint8_t label_len);

private:
    // serialize the model into buf (>= BLOB_MAX), returns bytes written
    uint16_t serialize(uint8_t *buf) const;
    // parse a blob into the model, false if magic/version/counts/crc are bad
    bool deserialize(const uint8_t *buf, uint16_t len);

    Band _bands[MAX_BANDS];
    PowerLevel _power[MAX_POWER_LEVELS];
    uint8_t _num_bands;
    uint8_t _num_channels;
    uint8_t _num_power_levels;
};

#endif  // AP_VIDEOTX_TABLE_ENABLED
