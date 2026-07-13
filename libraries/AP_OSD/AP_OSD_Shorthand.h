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
  User-definable OSD message shorthand: a small table of from->to text
  substitutions applied to the MESSAGE panel on top of the built-in dictionary,
  so the pilot can author their own abbreviations. Held in RAM here; a
  StorageManager blob provides persistence and the @OSD MAVLink-FTP mount lets a
  ground station edit the whole table. Modelled on AP_VideoTX_Table.
*/
#pragma once

#include "AP_OSD_config.h"

#if OSD_ENABLED

#include <AP_HAL/AP_HAL.h>
#include <AP_Common/AP_Common.h>

class AP_OSD_Shorthand {
public:
    static const uint8_t  MAX_ENTRIES = 16;
    static const uint8_t  FROM_LEN = 16;   // NUL-terminated within the field
    static const uint8_t  TO_LEN   = 10;   // NUL-terminated within the field

    struct Entry {
        char from[FROM_LEN];   // matched (case-insensitive) against the message
        char to[TO_LEN];       // replacement text
    };

    // wire / storage constants
    static const uint16_t BLOB_MAGIC = 0x534F;  // 'OS'
    static const uint8_t  BLOB_VERSION = 1;
    static const uint16_t BLOB_MAX = 4 + MAX_ENTRIES*(FROM_LEN+TO_LEN) + 4;

    AP_OSD_Shorthand() {}
    CLASS_NO_COPY(AP_OSD_Shorthand);

    // load the table from storage (empty if absent/invalid). Call once at boot.
    void init();
    // persist the current table; false if storage is unavailable
    bool save();

    // apply the user substitutions to buf in place. buf is a C string of at most
    // (size-1) chars; replacements are bounded so it never overflows size.
    void apply(char *buf, size_t size) const;

    uint8_t num_entries() const { return _num_entries; }
    bool is_empty() const { return _num_entries == 0; }

    // serialize into buf (>= BLOB_MAX), returns bytes written (for @OSD FTP read)
    uint16_t to_blob(uint8_t *buf) const { return serialize(buf); }
    // replace the table from a serialized blob and persist it; false (table
    // unchanged) if the blob is malformed
    bool from_blob(const uint8_t *buf, uint16_t len) {
        if (!deserialize(buf, len)) {
            return false;
        }
        save();
        return true;
    }

private:
    uint16_t serialize(uint8_t *buf) const;
    bool deserialize(const uint8_t *buf, uint16_t len);

    Entry _entries[MAX_ENTRIES];
    uint8_t _num_entries;
};

#endif  // AP_OSD_ENABLED
