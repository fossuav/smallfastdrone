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

#include "AP_OSD_Shorthand.h"

#if OSD_ENABLED

#include "AP_OSD_Message.h"
#include <AP_Math/AP_Math.h>
#include <AP_Math/crc.h>
#include <StorageManager/StorageManager.h>
#include <string.h>
#include <ctype.h>

// copy src into dst (size dstsize) upper-cased and NUL-terminated; the OSD font
// has no lower-case glyphs so replacements are emitted upper-case
static void to_upper_copy(char *dst, const char *src, size_t dstsize)
{
    size_t i = 0;
    for (; src[i] != 0 && i + 1 < dstsize; i++) {
        dst[i] = toupper((unsigned char)src[i]);
    }
    dst[i] = 0;
}

void AP_OSD_Shorthand::apply(char *buf, size_t size) const
{
    for (uint8_t i = 0; i < _num_entries; i++) {
        if (_entries[i].from[0] == 0) {
            continue;
        }
        char ufrom[FROM_LEN];
        char uto[TO_LEN];
        to_upper_copy(ufrom, _entries[i].from, sizeof(ufrom));
        to_upper_copy(uto, _entries[i].to, sizeof(uto));
        // str_replace() is shorten-only: a replacement longer than its key is
        // ignored, so the message length is monotonically non-increasing and can
        // never grow or overflow buf, regardless of what a ground station uploads.
        AP_OSD_Msg::str_replace(buf, ufrom, uto);
    }
    (void)size;
}

uint16_t AP_OSD_Shorthand::serialize(uint8_t *buf) const
{
    uint16_t o = 0;
    buf[o++] = BLOB_MAGIC & 0xFF;
    buf[o++] = BLOB_MAGIC >> 8;
    buf[o++] = BLOB_VERSION;
    buf[o++] = _num_entries;
    for (uint8_t i = 0; i < _num_entries; i++) {
        memcpy(&buf[o], _entries[i].from, FROM_LEN);
        o += FROM_LEN;
        memcpy(&buf[o], _entries[i].to, TO_LEN);
        o += TO_LEN;
    }
    const uint32_t crc = crc_crc32(0, buf, o);
    buf[o++] = crc & 0xFF;
    buf[o++] = (crc >> 8) & 0xFF;
    buf[o++] = (crc >> 16) & 0xFF;
    buf[o++] = (crc >> 24) & 0xFF;
    return o;
}

bool AP_OSD_Shorthand::deserialize(const uint8_t *buf, uint16_t len)
{
    if (len < 4) {
        return false;
    }
    if (uint16_t(buf[0] | (buf[1] << 8)) != BLOB_MAGIC || buf[2] != BLOB_VERSION) {
        return false;
    }
    const uint8_t n = buf[3];
    if (n > MAX_ENTRIES) {
        return false;
    }
    const uint16_t need = 4 + n * (FROM_LEN + TO_LEN) + 4;
    if (need > len) {
        return false;
    }
    const uint32_t crc = crc_crc32(0, buf, need - 4);
    const uint32_t stored = buf[need-4] | (buf[need-3] << 8) | (buf[need-2] << 16) | (uint32_t(buf[need-1]) << 24);
    if (crc != stored) {
        return false;
    }
    // valid: load it (NUL-terminate the fixed fields defensively)
    memset(_entries, 0, sizeof(_entries));
    uint16_t o = 4;
    for (uint8_t i = 0; i < n; i++) {
        memcpy(_entries[i].from, &buf[o], FROM_LEN);
        _entries[i].from[FROM_LEN-1] = 0;
        o += FROM_LEN;
        memcpy(_entries[i].to, &buf[o], TO_LEN);
        _entries[i].to[TO_LEN-1] = 0;
        o += TO_LEN;
    }
    _num_entries = n;
    return true;
}

void AP_OSD_Shorthand::init()
{
    StorageAccess storage(StorageManager::StorageOSDShorthand);
    if (storage.size() >= 4) {
        uint8_t buf[BLOB_MAX];
        const uint16_t n = MIN(uint16_t(storage.size()), uint16_t(BLOB_MAX));
        if (storage.read_block(buf, 0, n) && deserialize(buf, n)) {
            return;  // valid stored table loaded
        }
    }
    // absent or invalid: start empty (built-in dictionary still applies)
    memset(_entries, 0, sizeof(_entries));
    _num_entries = 0;
}

bool AP_OSD_Shorthand::save()
{
    StorageAccess storage(StorageManager::StorageOSDShorthand);
    uint8_t buf[BLOB_MAX];
    const uint16_t len = serialize(buf);
    if (storage.size() < len) {
        return false;   // no region (small-storage board): stays RAM-only
    }
    return storage.write_block(0, buf, len);
}

#endif  // OSD_ENABLED
