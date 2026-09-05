/*
  support checking board ID and firmware CRC in the bootloader
 */
#include "AP_CheckFirmware.h"
#include <AP_HAL/HAL.h>
#include <AP_Math/crc.h>

#if AP_CHECK_FIRMWARE_ENABLED

#if defined(HAL_BOOTLOADER_BUILD)

#if AP_SIGNED_FIRMWARE
#include "../../Tools/AP_Bootloader/support.h"
#include <string.h>
#include "monocypher.h"

const struct ap_secure_data public_keys __attribute__((section(".apsec_data")));

/*
  return true if all public keys are zero. We allow boot of an
  unsigned firmware in that case
 */
static bool all_zero_public_keys(void)
{
    /*
      look over all public keys, if one matches then we are OK
     */
    const uint8_t zero_key[AP_PUBLIC_KEY_LEN] {};
    for (const auto &public_key : public_keys.public_key) {
        if (memcmp(public_key.key, zero_key, AP_PUBLIC_KEY_LEN) != 0) {
            return false;
        }
    }
    return true;
}

/*
  return true if this board has been given an identity.

  Read straight out of the bootloader's own copy of the secure data: the
  identity is the last member of ap_secure_data, so it is already here and
  needs no help from the firmware to reach.
 */
static bool has_identity(void)
{
    // no signature check here, unlike the firmware's find_identity(). The
    // firmware has to locate the region inside a bootloader image it read
    // out of flash, and the signature is how it tells the region from
    // code that happens to sit at that offset. Here public_keys *is* this
    // bootloader's own .apsec_data, placed by the linker, so the region is
    // wherever the struct says it is. Comparing the signature would also
    // put a second copy of that byte pattern in the image, which is the
    // opposite of what a unique marker is for.
    const uint8_t zero_key[AP_IDENTITY_KEY_LEN] {};
    return memcmp(public_keys.identity.private_key, zero_key, AP_IDENTITY_KEY_LEN) != 0;
}

/*
  may this board boot firmware that carries no valid signature?

  An empty key set means an unsecured board, and booting unsigned firmware
  there is deliberate - it is how a board is bootstrapped, and how an owner
  removes signing entirely. But a board holding an identity was keyed when
  that identity was written, so an empty key set on one of those means the
  keys have since been removed, which is exactly the downgrade the
  signing exists to prevent. Refuse.

  This is held in the bootloader on purpose. A tool-side guard protects
  against a buggy configurator; only the bootloader protects against a
  hostile one, because it is the one thing an attacker with the drone in
  their hands cannot talk to.
 */
static bool unsigned_boot_allowed(void)
{
    return all_zero_public_keys() && !has_identity();
}

/*
  check a signature against bootloader keys
 */
static check_fw_result_t check_firmware_signature(const app_descriptor_signed *ad,
                                                  const uint8_t *flash1, uint32_t len1,
                                                  const uint8_t *flash2, uint32_t len2)
{
    if (unsigned_boot_allowed()) {
        return check_fw_result_t::CHECK_FW_OK;
    }

    // 8 byte signature version
    static const uint64_t sig_version = 30437LLU;
    if (ad->signature_length != 72) {
        return check_fw_result_t::FAIL_REASON_BAD_FIRMWARE_SIGNATURE;
    }
    if (memcmp((const uint8_t*)&sig_version, ad->signature, sizeof(sig_version)) != 0) {
        return check_fw_result_t::FAIL_REASON_BAD_FIRMWARE_SIGNATURE;
    }

    /*
      look over all public keys, if one matches then we are OK
     */
    for (const auto &public_key : public_keys.public_key) {
        crypto_check_ctx ctx {};
        crypto_check_ctx_abstract *actx = (crypto_check_ctx_abstract*)&ctx;
        crypto_check_init(actx, &ad->signature[sizeof(sig_version)], public_key.key);

        crypto_check_update(actx, flash1, len1);
        crypto_check_update(actx, flash2, len2);
        if (crypto_check_final(actx) == 0) {
            // good signature
            return check_fw_result_t::CHECK_FW_OK;
        }
    }

    // none of the public keys matched
    return check_fw_result_t::FAIL_REASON_VERIFICATION;
}
#endif // AP_SIGNED_FIRMWARE

/*
  check firmware CRC and board ID to see if it matches
 */
static check_fw_result_t check_good_firmware_signed(void)
{
    const uint8_t sig[8] = AP_APP_DESCRIPTOR_SIGNATURE_SIGNED;
    const uint8_t *flash1 = (const uint8_t *)(FLASH_LOAD_ADDRESS + (FLASH_BOOTLOADER_LOAD_KB + APP_START_OFFSET_KB)*1024);
    const uint32_t flash_size = (BOARD_FLASH_SIZE - (FLASH_BOOTLOADER_LOAD_KB + APP_START_OFFSET_KB))*1024;
    const app_descriptor_signed *ad = (const app_descriptor_signed *)memmem(flash1, flash_size-sizeof(app_descriptor_signed), sig, sizeof(sig));
    if (ad == nullptr) {
        // no application signature
        return check_fw_result_t::FAIL_REASON_NO_APP_SIG;
    }
    // check length
    if (ad->image_size > flash_size) {
        return check_fw_result_t::FAIL_REASON_BAD_LENGTH_APP;
    }

    bool id_ok = (ad->board_id == APJ_BOARD_ID);
#ifdef ALT_BOARD_ID
    id_ok |= (ad->board_id == ALT_BOARD_ID);
#endif

    if (!id_ok) {
        return check_fw_result_t::FAIL_REASON_BAD_BOARD_ID;
    }

    const uint8_t *flash2 = (const uint8_t *)&ad->version_major;
    const uint8_t desc_len = offsetof(app_descriptor_signed, version_major) - offsetof(app_descriptor_signed, image_crc1);
    const uint32_t len1 = ((const uint8_t *)&ad->image_crc1) - flash1;

    if ((len1 + desc_len) > ad->image_size) {
        return check_fw_result_t::FAIL_REASON_BAD_LENGTH_DESCRIPTOR;
    }

    const uint32_t len2 = ad->image_size - (len1 + desc_len);
    uint32_t crc1 = crc32_small(0, flash1, len1);
    uint32_t crc2 = crc32_small(0, flash2, len2);
    if (crc1 != ad->image_crc1 || crc2 != ad->image_crc2) {
        return check_fw_result_t::FAIL_REASON_BAD_CRC;
    }

    check_fw_result_t ret = check_fw_result_t::CHECK_FW_OK;

#if AP_SIGNED_FIRMWARE
    ret = check_firmware_signature(ad, flash1, len1, flash2, len2);
#endif

    return ret;
}

/*
  check firmware CRC and board ID to see if it matches, using unsigned
  signature
 */
static check_fw_result_t check_good_firmware_unsigned(void)
{
    const uint8_t sig[8] = AP_APP_DESCRIPTOR_SIGNATURE_UNSIGNED;
    const uint8_t *flash1 = (const uint8_t *)(FLASH_LOAD_ADDRESS + (FLASH_BOOTLOADER_LOAD_KB + APP_START_OFFSET_KB)*1024);
    const uint32_t flash_size = (BOARD_FLASH_SIZE - (FLASH_BOOTLOADER_LOAD_KB + APP_START_OFFSET_KB))*1024;
    const app_descriptor_unsigned *ad = (const app_descriptor_unsigned *)memmem(flash1, flash_size-sizeof(app_descriptor_unsigned), sig, sizeof(sig));
    if (ad == nullptr) {
        // no application signature
        return check_fw_result_t::FAIL_REASON_NO_APP_SIG;
    }
    // check length
    if (ad->image_size > flash_size) {
        return check_fw_result_t::FAIL_REASON_BAD_LENGTH_APP;
    }

    bool id_ok = (ad->board_id == APJ_BOARD_ID);
#ifdef ALT_BOARD_ID
    id_ok |= (ad->board_id == ALT_BOARD_ID);
#endif

    if (!id_ok) {
        return check_fw_result_t::FAIL_REASON_BAD_BOARD_ID;
    }

    const uint8_t *flash2 = (const uint8_t *)&ad->version_major;
    const uint8_t desc_len = offsetof(app_descriptor_unsigned, version_major) - offsetof(app_descriptor_unsigned, image_crc1);
    const uint32_t len1 = ((const uint8_t *)&ad->image_crc1) - flash1;

    if ((len1 + desc_len) > ad->image_size) {
        return check_fw_result_t::FAIL_REASON_BAD_LENGTH_DESCRIPTOR;
    }

    const uint32_t len2 = ad->image_size - (len1 + desc_len);
    uint32_t crc1 = crc32_small(0, flash1, len1);
    uint32_t crc2 = crc32_small(0, flash2, len2);
    if (crc1 != ad->image_crc1 || crc2 != ad->image_crc2) {
        return check_fw_result_t::FAIL_REASON_BAD_CRC;
    }

    return check_fw_result_t::CHECK_FW_OK;
}

check_fw_result_t check_good_firmware(void)
{
#if AP_SIGNED_FIRMWARE
    // allow unsigned format if we have no public keys. This allows
    // for use of SECURE_COMMAND to remove all public keys and then
    // load of unsigned firmware
    const auto ret = check_good_firmware_signed();
    if (ret != check_fw_result_t::CHECK_FW_OK &&
        unsigned_boot_allowed() &&
        check_good_firmware_unsigned() == check_fw_result_t::CHECK_FW_OK) {
        return check_fw_result_t::CHECK_FW_OK;
    }
    return ret;
#else
    const auto ret = check_good_firmware_unsigned();
    if (ret != check_fw_result_t::CHECK_FW_OK) {
        // allow for signed format, not checking public keys. This
        // allows for booting of a signed firmware with an unsigned
        // bootloader, which allows for bootstrapping a system up from
        // unsigned to signed
        const auto ret2 = check_good_firmware_signed();
        if (ret2 == check_fw_result_t::CHECK_FW_OK) {
            return check_fw_result_t::CHECK_FW_OK;
        }
    }
    return ret;
#endif
}

const app_descriptor_t *get_app_descriptor(void)
{
#if AP_SIGNED_FIRMWARE
    const uint8_t sig[8] = AP_APP_DESCRIPTOR_SIGNATURE_SIGNED;
#else
    const uint8_t sig[8] = AP_APP_DESCRIPTOR_SIGNATURE_UNSIGNED;
#endif
    const uint8_t *flash1 = (const uint8_t *)(FLASH_LOAD_ADDRESS + (FLASH_BOOTLOADER_LOAD_KB + APP_START_OFFSET_KB)*1024);
    const uint32_t flash_size = (BOARD_FLASH_SIZE - (FLASH_BOOTLOADER_LOAD_KB + APP_START_OFFSET_KB))*1024;
    const app_descriptor_t *ad = (const app_descriptor_t *)memmem(flash1, flash_size-sizeof(app_descriptor_t), sig, sizeof(sig));
    return ad;
}

#endif // HAL_BOOTLOADER_BUILD

#if !defined(HAL_BOOTLOADER_BUILD)
extern const AP_HAL::HAL &hal;
extern const app_descriptor_t app_descriptor;

/*
  this is needed to ensure we don't elide the app_descriptor
 */
void check_firmware_print(void)
{
    hal.console->printf("Booting %u/%u\n",
                        app_descriptor.version_major,
                        app_descriptor.version_minor);
}
#endif


#endif // AP_CHECK_FIRMWARE_ENABLED
