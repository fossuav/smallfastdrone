#pragma once

#include <AP_HAL/AP_HAL_Boards.h>
#include <AP_SerialManager/AP_SerialManager_config.h>
#include <AP_Vehicle/AP_Vehicle_config.h>
#include <AP_CheckFirmware/AP_CheckFirmware_config.h>

#ifndef AP_SCRIPTING_ENABLED
#define AP_SCRIPTING_ENABLED (HAL_PROGRAM_SIZE_LIMIT_KB > 1024)
#endif

#if AP_SCRIPTING_ENABLED
    #include <AP_Filesystem/AP_Filesystem_config.h>
    // enumerate all of the possible places we can read a script from.
    #if !AP_FILESYSTEM_POSIX_ENABLED && !AP_FILESYSTEM_FATFS_ENABLED && !AP_FILESYSTEM_ESP32_ENABLED && !AP_FILESYSTEM_ROMFS_ENABLED && !AP_FILESYSTEM_LITTLEFS_ENABLED
        #error "Scripting requires a filesystem"
    #endif
#endif

#ifndef AP_SCRIPTING_SERIALDEVICE_ENABLED
#define AP_SCRIPTING_SERIALDEVICE_ENABLED AP_SERIALMANAGER_REGISTER_ENABLED && (HAL_PROGRAM_SIZE_LIMIT_KB>1024)
#endif

// bindings configuration
#ifndef AP_SCRIPTING_BINDING_MOTORS_ENABLED
#define AP_SCRIPTING_BINDING_MOTORS_ENABLED (AP_SCRIPTING_ENABLED && AP_VEHICLE_ENABLED)
#endif

#ifndef AP_SCRIPTING_BINDING_VEHICLE_ENABLED
#define AP_SCRIPTING_BINDING_VEHICLE_ENABLED 1
#endif  // AP_SCRIPTING_BINDING_VEHICLE_ENABLED

#ifndef AP_SCRIPTING_ENCRYPTION_ENABLED
#define AP_SCRIPTING_ENCRYPTION_ENABLED AP_SCRIPTING_ENABLED && AP_CHECK_FIRMWARE_ENABLED && AP_SIGNED_FIRMWARE
#endif

// how much of the nonce carries the board id. The STM32 unique id is 12
// bytes and get_system_id_unformatted() caps at that
#define AP_SCRIPTING_NONCE_UID_LEN 12

/*
  .lxa v2 layout. v1 encrypted with a bootloader *public* key used as a
  symmetric key, which is obfuscation rather than encryption - that key is
  in every bootloader, so anyone holding the firmware could read the
  script. v2 agrees a key with the drone's own identity instead, so a
  script can be encrypted for one airframe and no other.

    0    6   magic
    6   12   target board id, plaintext so a wrong file is refused cheaply
   18   32   sender's ephemeral X25519 public key
   50   24   nonce
   74   16   Poly1305 tag
   90  ...   XChaCha20-Poly1305 ciphertext

  The board id gets its own field rather than prefixing the nonce, which
  is what docs/SECURITY.md originally sketched: the nonce then stays fully
  random, and the check reads as the check it is. Flipping the id gains an
  attacker nothing - the key agreement is what actually gates decryption,
  and it does not involve this field.
 */
#define AP_SCRIPTING_LXA2_MAGIC "LXA2.0"
#define AP_SCRIPTING_LXA2_MAGIC_LEN 6
#define AP_SCRIPTING_LXA2_EPK_LEN 32
#define AP_SCRIPTING_LXA2_NONCE_LEN 24
#define AP_SCRIPTING_LXA2_MAC_LEN 16
#define AP_SCRIPTING_LXA2_HEADER_LEN (AP_SCRIPTING_LXA2_MAGIC_LEN + \
                                      AP_SCRIPTING_NONCE_UID_LEN + \
                                      AP_SCRIPTING_LXA2_EPK_LEN + \
                                      AP_SCRIPTING_LXA2_NONCE_LEN + \
                                      AP_SCRIPTING_LXA2_MAC_LEN)

#ifndef AP_SCRIPTING_ENCRYPTION_UUID_ENABLED
#define AP_SCRIPTING_ENCRYPTION_UUID_ENABLED 0
#endif
