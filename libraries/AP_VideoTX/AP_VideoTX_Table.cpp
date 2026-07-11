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

#include "AP_VideoTX_Table.h"

#if AP_VIDEOTX_TABLE_ENABLED

#include <AP_Math/AP_Math.h>
#include <AP_Math/crc.h>
#include <StorageManager/StorageManager.h>
#include <string.h>

// compiled-in default table, seeded when no user table is stored. These are the
// historical AP_VideoTX bands/frequencies, so band indices remain compatible
// with AP_VideoTX::VideoBand and behaviour is unchanged out of the box.
struct DefaultBand {
    const char *name;
    char letter;
    uint16_t freq[AP_VideoTX_Table::MAX_CHANNELS];
};

static const DefaultBand default_bands[] = {
    { "BOSCAM_A", 'A', { 5865, 5845, 5825, 5805, 5785, 5765, 5745, 5725 } },
    { "BOSCAM_B", 'B', { 5733, 5752, 5771, 5790, 5809, 5828, 5847, 5866 } },
    { "BOSCAM_E", 'E', { 5705, 5685, 5665, 5645, 5885, 5905, 5925, 5945 } },
    { "FATSHARK", 'F', { 5740, 5760, 5780, 5800, 5820, 5840, 5860, 5880 } },
    { "RACEBAND", 'R', { 5658, 5695, 5732, 5769, 5806, 5843, 5880, 5917 } },
    { "LOWRACE",  'L', { 5362, 5399, 5436, 5473, 5510, 5547, 5584, 5621 } },
    { "1G3_A",    'U', { 1080, 1120, 1160, 1200, 1240, 1280, 1320, 1360 } },
    { "1G3_B",    'V', { 1080, 1120, 1160, 1200, 1258, 1280, 1320, 1360 } },
    { "BAND_X",   'X', { 4990, 5020, 5050, 5080, 5110, 5140, 5170, 5200 } },
    { "3G3_A",    'C', { 3330, 3350, 3370, 3390, 3410, 3430, 3450, 3470 } },
    { "3G3_B",    'D', { 3170, 3190, 3210, 3230, 3250, 3270, 3290, 3310 } },
};

// standard analog power set as a starting point. "value" is protocol-dependent
// (mW for Tramp, index/dBm for SmartAudio) and is refined by the power-table
// integration; label is what the OSD/configurator shows.
struct DefaultPower {
    uint16_t value;
    const char *label;
};

static const DefaultPower default_powers[] = {
    {   25, "25"  },
    {  100, "100" },
    {  200, "200" },
    {  400, "400" },
    {  500, "500" },
    {  600, "600" },
    {  800, "800" },
    { 1000, "1W"  },
};

// copy a C string into a fixed, non-NUL-terminated field, zero padding the rest
static void set_fixed(char *dst, size_t dst_len, const char *src)
{
    memset(dst, 0, dst_len);
    strncpy(dst, src, dst_len);
}

// copy a fixed, possibly non-NUL-terminated field out as a NUL-terminated string
static void get_fixed(const char *src, size_t src_len, char *out, size_t out_len)
{
    if (out_len == 0) {
        return;
    }
    size_t n = 0;
    while (n < src_len && n + 1 < out_len && src[n] != '\0') {
        out[n] = src[n];
        n++;
    }
    out[n] = '\0';
}

void AP_VideoTX_Table::load_defaults()
{
    memset(_bands, 0, sizeof(_bands));
    memset(_power, 0, sizeof(_power));

    _num_bands = MIN(uint8_t(ARRAY_SIZE(default_bands)), MAX_BANDS);
    _num_channels = MAX_CHANNELS;
    for (uint8_t b = 0; b < _num_bands; b++) {
        set_fixed(_bands[b].name, BAND_NAME_LEN, default_bands[b].name);
        _bands[b].letter = default_bands[b].letter;
        _bands[b].is_factory = true;  // standard bands: VTX may use its own map
        for (uint8_t c = 0; c < MAX_CHANNELS; c++) {
            _bands[b].freq[c] = default_bands[b].freq[c];
        }
    }

    _num_power_levels = MIN(uint8_t(ARRAY_SIZE(default_powers)), MAX_POWER_LEVELS);
    for (uint8_t i = 0; i < _num_power_levels; i++) {
        _power[i].value = default_powers[i].value;
        set_fixed(_power[i].label, POWER_LABEL_LEN, default_powers[i].label);
    }
}

uint16_t AP_VideoTX_Table::frequency(uint8_t band, uint8_t channel) const
{
    if (band >= _num_bands || channel >= _num_channels) {
        return 0;
    }
    return _bands[band].freq[channel];
}

bool AP_VideoTX_Table::band_and_channel_for_frequency(uint16_t freq, uint8_t &band, uint8_t &channel) const
{
    if (freq == 0) {
        return false;
    }
    for (uint8_t b = 0; b < _num_bands; b++) {
        for (uint8_t c = 0; c < _num_channels; c++) {
            if (_bands[b].freq[c] == freq) {
                band = b;
                channel = c;
                return true;
            }
        }
    }
    return false;
}

char AP_VideoTX_Table::band_letter(uint8_t band) const
{
    if (band >= _num_bands) {
        return '?';
    }
    return _bands[band].letter;
}

void AP_VideoTX_Table::band_name(uint8_t band, char *out, size_t out_len) const
{
    if (band >= _num_bands) {
        if (out_len > 0) {
            out[0] = '\0';
        }
        return;
    }
    get_fixed(_bands[band].name, BAND_NAME_LEN, out, out_len);
}

bool AP_VideoTX_Table::band_is_factory(uint8_t band) const
{
    if (band >= _num_bands) {
        return false;
    }
    return _bands[band].is_factory;
}

uint16_t AP_VideoTX_Table::power_value(uint8_t index) const
{
    if (index >= _num_power_levels) {
        return 0;
    }
    return _power[index].value;
}

void AP_VideoTX_Table::power_label(uint8_t index, char *out, size_t out_len) const
{
    if (index >= _num_power_levels) {
        if (out_len > 0) {
            out[0] = '\0';
        }
        return;
    }
    get_fixed(_power[index].label, POWER_LABEL_LEN, out, out_len);
}

// ---- persistence (compact little-endian binary blob + CRC32) ----

uint16_t AP_VideoTX_Table::serialize(uint8_t *buf) const
{
    uint16_t o = 0;
    buf[o++] = BLOB_MAGIC & 0xFF;
    buf[o++] = BLOB_MAGIC >> 8;
    buf[o++] = BLOB_VERSION;
    buf[o++] = _num_bands;
    buf[o++] = _num_channels;
    buf[o++] = _num_power_levels;
    for (uint8_t b = 0; b < _num_bands; b++) {
        memcpy(&buf[o], _bands[b].name, BAND_NAME_LEN);
        o += BAND_NAME_LEN;
        buf[o++] = uint8_t(_bands[b].letter);
        buf[o++] = _bands[b].is_factory ? 1 : 0;
        for (uint8_t c = 0; c < _num_channels; c++) {
            buf[o++] = _bands[b].freq[c] & 0xFF;
            buf[o++] = _bands[b].freq[c] >> 8;
        }
    }
    for (uint8_t i = 0; i < _num_power_levels; i++) {
        buf[o++] = _power[i].value & 0xFF;
        buf[o++] = _power[i].value >> 8;
        memcpy(&buf[o], _power[i].label, POWER_LABEL_LEN);
        o += POWER_LABEL_LEN;
    }
    const uint32_t crc = crc_crc32(0, buf, o);
    buf[o++] = crc & 0xFF;
    buf[o++] = (crc >> 8) & 0xFF;
    buf[o++] = (crc >> 16) & 0xFF;
    buf[o++] = (crc >> 24) & 0xFF;
    return o;
}

bool AP_VideoTX_Table::deserialize(const uint8_t *buf, uint16_t len)
{
    if (len < 6) {
        return false;
    }
    const uint16_t magic = buf[0] | (buf[1] << 8);
    if (magic != BLOB_MAGIC || buf[2] != BLOB_VERSION) {
        return false;
    }
    const uint8_t nb = buf[3];
    const uint8_t nc = buf[4];
    const uint8_t np = buf[5];
    if (nb > MAX_BANDS || nc > MAX_CHANNELS || np > MAX_POWER_LEVELS) {
        return false;
    }
    const uint16_t need = 6 + nb*(BAND_NAME_LEN+2+nc*2) + np*(2+POWER_LABEL_LEN) + 4;
    if (need > len) {
        return false;
    }
    const uint32_t stored = buf[need-4] | (buf[need-3] << 8) |
                            (buf[need-2] << 16) | (uint32_t(buf[need-1]) << 24);
    if (crc_crc32(0, buf, need-4) != stored) {
        return false;
    }
    // valid: commit into the model
    memset(_bands, 0, sizeof(_bands));
    memset(_power, 0, sizeof(_power));
    uint16_t o = 6;
    for (uint8_t b = 0; b < nb; b++) {
        memcpy(_bands[b].name, &buf[o], BAND_NAME_LEN);
        o += BAND_NAME_LEN;
        _bands[b].letter = char(buf[o++]);
        _bands[b].is_factory = buf[o++] != 0;
        for (uint8_t c = 0; c < nc; c++) {
            _bands[b].freq[c] = buf[o] | (buf[o+1] << 8);
            o += 2;
        }
    }
    for (uint8_t i = 0; i < np; i++) {
        _power[i].value = buf[o] | (buf[o+1] << 8);
        o += 2;
        memcpy(_power[i].label, &buf[o], POWER_LABEL_LEN);
        o += POWER_LABEL_LEN;
    }
    _num_bands = nb;
    _num_channels = nc;
    _num_power_levels = np;
    return true;
}

void AP_VideoTX_Table::init()
{
    StorageAccess storage(StorageManager::StorageVTXTable);
    if (storage.size() >= 6) {
        uint8_t buf[BLOB_MAX];
        const uint16_t n = MIN(uint16_t(storage.size()), uint16_t(BLOB_MAX));
        if (storage.read_block(buf, 0, n) && deserialize(buf, n)) {
            return;  // valid stored table loaded
        }
    }
    // absent or invalid: seed the defaults and persist them if we have room
    load_defaults();
    save();
}

bool AP_VideoTX_Table::save()
{
    StorageAccess storage(StorageManager::StorageVTXTable);
    uint8_t buf[BLOB_MAX];
    const uint16_t n = serialize(buf);
    if (storage.size() < n) {
        return false;  // no VTX-table region on this board (small storage)
    }
    return storage.write_block(0, buf, n);
}

#endif  // AP_VIDEOTX_TABLE_ENABLED
