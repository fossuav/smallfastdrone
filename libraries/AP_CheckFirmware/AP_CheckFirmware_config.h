#pragma once

#include <AP_HAL/AP_HAL_Boards.h>
#include <AP_OpenDroneID/AP_OpenDroneID_config.h>

#ifndef AP_CHECK_FIRMWARE_ENABLED
#define AP_CHECK_FIRMWARE_ENABLED AP_OPENDRONEID_ENABLED
#endif

// fix the public key set at build time: key management over MAVLink is
// compiled out and an empty key set refuses secure commands instead of
// accepting them all. Set from hwdef on boards that ship keyed
#ifndef AP_CHECK_FIRMWARE_FIXED_KEYS
#define AP_CHECK_FIRMWARE_FIXED_KEYS 0
#endif

// per-drone identity generated on the board and read back over
// SECURE_COMMAND (GENERATE_IDENTITY / GET_IDENTITY), plus the owner key
// commands that go with it.
//
// AP_SIGNED_FIRMWARE is part of the condition because everything this
// gates lives in the bootloader's secure data, and the accessors that
// reach it are themselves compiled out of an unsigned build. A hwdef
// sets FIXED_KEYS unconditionally, so without this an unsigned build of
// a product board does not compile
#ifndef AP_CHECK_FIRMWARE_IDENTITY_ENABLED
#define AP_CHECK_FIRMWARE_IDENTITY_ENABLED (AP_CHECK_FIRMWARE_FIXED_KEYS && AP_SIGNED_FIRMWARE)
#endif
