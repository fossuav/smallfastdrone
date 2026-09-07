# User-definable VTX tables

A Betaflight-style, user-definable table of video-transmitter **bands /
frequencies** and **power levels**, replacing the previously hardcoded band grid
and static power set. The table is the single source of truth for
band/channel → frequency and for the selectable power set, is persisted on the
flight controller, and is read/written whole over MAVLink FTP.

## Why

VTX hardware varies: different bands/channel frequencies (region and model
specific) and, especially, different **power levels**. Generic firmware that
guesses a fixed set (25/100/200/400/…) offers levels a given VTX may reject.
A user table lets the firmware know the VTX's *real* capabilities, so band and
power selection always match the hardware.

## Data model (`AP_VideoTX_Table`)

Two decoupled tables. Limits: **12 bands × 8 channels × 8 power levels**;
band name 8 chars, band letter 1 char, power label 3 chars.

- **Bands** — each: name, single-letter id, `is_factory` flag, and up to 8
  channel frequencies in MHz (`0` = channel disabled). `band+channel` resolves
  to a frequency; a reverse lookup maps a frequency back to band/channel.
  `is_factory` marks a standard band whose frequencies a VTX may drive from its
  own internal map (vs a custom band whose literal frequencies are sent).
- **Power levels** — each: a protocol **value** and a display **label**. The
  value is what the active VTX protocol expects on the wire:
  - **Tramp / IRC Tramp** — value = **mW**, sent verbatim.
  - **SmartAudio** — value is treated as mW here and converted to the level /
    dBm / DAC the protocol version needs.
  The **label** is the decoupled display string (OSD / configurator), e.g.
  `"25"`, `"400"`, `"1.6"`. Power is selected by index over the levels.

When no table is stored the model is seeded from the historical compiled-in
bands and a standard power set, so behaviour is unchanged out of the box.

## Persistence

The table is stored as a compact binary blob in a dedicated `StorageVTXTable`
region (see `StorageManager`). No SD card or filesystem is required. On boards
below the 32 KB storage tier the region is absent and the table falls back to
the seeded RAM defaults (no persistence).

## Wire format (`@VTX/vtxtable.dat`)

The table is exposed as a single virtual file over **MAVLink FTP** at
`@VTX/vtxtable.dat`. A ground station reads the whole blob, edits it, and writes
it back; on write the blob is validated (CRC) and committed/persisted, or
rejected wholesale if malformed (the existing table is left untouched).

Blob layout — little-endian, no padding:

| offset | field | type | notes |
|---|---|---|---|
| 0 | magic | u16 | `0x5654` ("VT") |
| 2 | version | u8 | `1` |
| 3 | num_bands | u8 | ≤ 12 |
| 4 | num_channels | u8 | ≤ 8 |
| 5 | num_power_levels | u8 | ≤ 8 |
| 6 | bands[num_bands] | — | see below |
| … | powers[num_power_levels] | — | see below |
| … | crc | u32 | see CRC note |

Per band (`6 + num_channels*2 + 2` bytes):

| field | type | notes |
|---|---|---|
| name | char[8] | zero-padded, not necessarily NUL-terminated |
| letter | char | single-char id |
| is_factory | u8 | 0 = custom, 1 = factory |
| freq[num_channels] | u16 each | MHz, 0 = channel disabled |

Per power level (5 bytes):

| field | type | notes |
|---|---|---|
| value | u16 | protocol value (mW for Tramp) |
| label | char[3] | zero-padded display string |

**CRC**: a 32-bit CRC over every byte before it, using ArduPilot's `crc_crc32`
(the standard reflected CRC-32 table, initial value `0`, **no** final XOR — this
is *not* `zlib.crc32`, which XORs the result). Reference:

```python
_TAB = []
for n in range(256):
    c = n
    for _ in range(8):
        c = (0xEDB88320 ^ (c >> 1)) if (c & 1) else (c >> 1)
    _TAB.append(c)
def ap_crc32(data, crc=0):
    for b in data:
        crc = _TAB[(crc ^ b) & 0xff] ^ (crc >> 8)
    return crc & 0xffffffff
```

## Selecting band / channel / power

The existing `VTX_*` parameters select within the table:

- `VTX_BAND` / `VTX_CHANNEL` — index into the table; `VTX_FREQ` is derived.
- `VTX_POWER` — power in mW (matched against the table's power values).
- `VTX_MAX_POWER` — caps the 6-position power switch and the over-power warning;
  raise it to allow higher levels (e.g. 1600 for a 1.6 W VTX).

The table drives the 6-position `VTX_POWER` RC switch (`RCn_OPTION = 94`),
CRSF/MSP power index, and OSD labels. Tramp's direct `VTX_POWER` is sent to the
device as raw mW.

## Configurator flow

1. FTP **GET** `@VTX/vtxtable.dat`, parse the blob.
2. Edit bands / power levels; recompute the CRC.
3. FTP **PUT** the whole blob back to `@VTX/vtxtable.dat` (validated + persisted
   on close).
