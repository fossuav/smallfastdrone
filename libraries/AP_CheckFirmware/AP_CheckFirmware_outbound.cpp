/*
  wrap an artefact the drone sends out so that only its owner can read
  it - the mirror of the .lxa path, which wraps what SFD sends in
 */
#include "AP_CheckFirmware.h"
#include <AP_HAL/HAL.h>

#if AP_CHECK_FIRMWARE_IDENTITY_ENABLED && !defined(HAL_BOOTLOADER_BUILD)

#include "monocypher.h"

extern const AP_HAL::HAL &hal;

/*
  fill in an artefact's header and derive the key its body is encrypted
  with.

  The key mixes two agreements. The ephemeral private key against the
  owner's public key gives a secret only the owner can re-derive, from
  the ephemeral public key written into the header. The identity
  private key against the same owner key gives one only this board can
  produce, which is what makes the artefact authentic rather than
  merely unreadable. That second agreement is static on both sides and
  would otherwise be one fixed key for the life of the drone; mixing
  the fresh one in with it is what keeps every artefact's key distinct.

  The first block of keystream becomes the Poly1305 key and the body
  starts at the next block, so the tag and the body never share one.
 */
bool AP_CheckFirmware::outbound_begin(uint8_t header[AP_SFX_HEADER_LEN], struct sfx_state &state,
                                      uint8_t content_type, uint8_t flags)
{
    const struct ap_identity_data *identity = find_identity();
    const struct ap_owner_data *owner = find_owner_key();
    if (!identity_is_set(identity) || !owner_key_is_set(owner)) {
        // an unclaimed drone, which is the ordinary state of one that
        // has never been enabled. Not an error
        return false;
    }

    uint8_t e_priv[AP_SFX_KEY_LEN];
    if (!hal.util->get_true_random_vals(e_priv, sizeof(e_priv), 100000)) {
        return false;
    }
    // X25519 clamp, so the scalar stored is the scalar used
    e_priv[0] &= 248;
    e_priv[31] &= 127;
    e_priv[31] |= 64;

    memset(header, 0, AP_SFX_HEADER_LEN);
    memcpy(header, AP_SFX_MAGIC, AP_SFX_MAGIC_LEN);
    header[AP_SFX_MAGIC_LEN] = content_type;
    header[AP_SFX_MAGIC_LEN+1] = flags;

    uint8_t uid_len = AP_IDENTITY_UID_LEN;
    if (!hal.util->get_system_id_unformatted(&header[AP_SFX_OFS_UID], uid_len) ||
        uid_len != AP_IDENTITY_UID_LEN) {
        crypto_wipe(e_priv, sizeof(e_priv));
        return false;
    }
    crypto_x25519_public_key(&header[AP_SFX_OFS_EPK], e_priv);

    // from the hardware RNG, not rand(). A nonce repeated under a key
    // that outlives one file is keystream reuse
    if (!hal.util->get_true_random_vals(&header[AP_SFX_OFS_NONCE], AP_SFX_NONCE_LEN, 100000)) {
        crypto_wipe(e_priv, sizeof(e_priv));
        return false;
    }

    uint8_t k_conf[AP_SFX_KEY_LEN];
    uint8_t k_auth[AP_SFX_KEY_LEN];
    crypto_key_exchange(k_conf, e_priv, owner->public_key);
    crypto_key_exchange(k_auth, identity->private_key, owner->public_key);
    crypto_wipe(e_priv, sizeof(e_priv));

    crypto_blake2b_ctx kdf;
    crypto_blake2b_general_init(&kdf, AP_SFX_KEY_LEN, nullptr, 0);
    crypto_blake2b_update(&kdf, k_conf, sizeof(k_conf));
    crypto_blake2b_update(&kdf, k_auth, sizeof(k_auth));
    crypto_blake2b_update(&kdf, (const uint8_t *)AP_SFX_CONTEXT, sizeof(AP_SFX_CONTEXT)-1);
    // magic, content type and flags, so the same drone and owner do not
    // derive one key for a log and a parameter set
    crypto_blake2b_update(&kdf, header, AP_SFX_MAGIC_LEN + 2);
    crypto_blake2b_final(&kdf, state.key);
    crypto_wipe(k_conf, sizeof(k_conf));
    crypto_wipe(k_auth, sizeof(k_auth));
    crypto_wipe(&kdf, sizeof(kdf));

    memcpy(state.nonce, &header[AP_SFX_OFS_NONCE], AP_SFX_NONCE_LEN);
    state.ctr = AP_SFX_BODY_BLOCK;

    /*
      the header is additional data with no plaintext of its own - the
      body is a separate stream - so the tag covers the ephemeral key,
      the board id and the flags, and nothing else. Driving the
      library's AEAD rather than Poly1305 by hand also puts the
      one-time key in block 0 where it belongs, which is why the body
      starts at block 1
     */
    crypto_lock_aead(&header[AP_SFX_OFS_MAC], nullptr, state.key, state.nonce,
                     header, AP_SFX_OFS_MAC, nullptr, 0);

    return true;
}

/*
  encrypt part of the body in place. Seekable, so the caller can hand
  over whatever it happens to have and pick up where it left off, and
  the ciphertext is the same length as the plaintext
 */
void AP_CheckFirmware::outbound_encrypt(struct sfx_state &state, uint8_t *buf, uint32_t len)
{
    state.ctr = crypto_xchacha20_ctr(buf, buf, len, state.key, state.nonce, state.ctr);
}

#endif // AP_CHECK_FIRMWARE_IDENTITY_ENABLED && !HAL_BOOTLOADER_BUILD
