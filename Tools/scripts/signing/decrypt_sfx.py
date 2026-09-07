#!/usr/bin/env python3
'''
read a .sfx artefact - a log or parameter set a drone encrypted to its
owner. The counterpart of encrypt_lua.py: that encrypts to a drone, this
decrypts what a drone sent out.

Needs two things. The owner's private key, which is the half that never
went near the drone, and the drone's identity file, because the artefact
is authenticated with the drone's identity key as well as concealed with
an ephemeral one - so a file cannot be read without knowing which drone
it claims to be from.

  decrypt_sfx.py --owner-key owner.bin --identity drone.json log.sfx -o log.bin
'''

import base64
import json
import sys
from argparse import ArgumentParser

import monocypher

MAGIC = b'SFDX10'
OFS_TYPE, OFS_FLAGS, OFS_UID, OFS_EPK, OFS_NONCE, OFS_MAC = 6, 7, 8, 20, 52, 76
HEADER_LEN = 92
CONTEXT = b'sfd-outbound/1'
BODY_BLOCK = 1
TYPES = {1: 'parameters', 2: 'log'}
FLAG_STREAM = 1


def content_key(owner_priv, ident_pub, epk, header):
    '''re-derive the key the drone encrypted with.

    Two agreements, mixed. Against the ephemeral public key in the file
    for confidentiality, and against the drone's identity public key for
    authenticity - the second is what no other drone could have
    produced. The drone computed the mirror of both.
    '''
    k_conf = monocypher.key_exchange(owner_priv, epk)
    k_auth = monocypher.key_exchange(owner_priv, ident_pub)
    kdf = monocypher.Blake2b(hash_size=32)
    kdf.update(k_conf)
    kdf.update(k_auth)
    kdf.update(CONTEXT)
    # magic, content type and flags, so a log and a parameter set from
    # the same drone do not share a key
    kdf.update(header[:OFS_UID])
    return kdf.finalize()


def keystream_from(key, nonce, block, length):
    '''XChaCha20 keystream from a given block. pymonocypher always
    starts at zero, so the earlier blocks are generated and dropped -
    block 0 is the AEAD one-time key and is never body keystream.'''
    skip = block * 64
    return monocypher.chacha20(key, nonce, b'\x00' * (skip + length))[skip:]


def decrypt(data, owner_priv, ident_pub):
    if len(data) <= HEADER_LEN or data[:len(MAGIC)] != MAGIC:
        raise ValueError('not a .sfx file')
    header, body = data[:HEADER_LEN], data[HEADER_LEN:]
    ctype, flags = header[OFS_TYPE], header[OFS_FLAGS]
    epk = header[OFS_EPK:OFS_EPK + 32]
    nonce = header[OFS_NONCE:OFS_NONCE + 24]

    key = content_key(owner_priv, ident_pub, epk, header)

    # the header is authenticated as additional data over an empty
    # message, which is what the drone did
    if monocypher.unlock(key, nonce, header[OFS_MAC:OFS_MAC + 16], b'',
                         associated_data=header[:OFS_MAC]) is None:
        raise ValueError('header does not authenticate - wrong owner key, '
                         'wrong drone identity, or a tampered header')

    plain = bytes(a ^ b for a, b in
                  zip(body, keystream_from(key, nonce, BODY_BLOCK, len(body))))
    return header, ctype, flags, plain


def main():
    ap = ArgumentParser(description=__doc__)
    ap.add_argument('--owner-key', required=True, help='32-byte owner private key')
    ap.add_argument('--identity', required=True, help="the drone's sfd-identity/1 file")
    ap.add_argument('-o', '--out', help='write plaintext here (default: stdout summary only)')
    ap.add_argument('file', help='the .sfx artefact')
    args = ap.parse_args()

    owner_priv = open(args.owner_key, 'rb').read()
    if len(owner_priv) != 32:
        sys.exit('owner key must be 32 bytes, got %u' % len(owner_priv))
    identity = json.load(open(args.identity))
    ident_pub = base64.b64decode(identity['public_key'])

    header, ctype, flags, plain = decrypt(open(args.file, 'rb').read(),
                                          owner_priv, ident_pub)
    uid = header[OFS_UID:OFS_UID + 12].hex()
    if identity.get('uid', uid).lower() != uid:
        print('warning: file is from drone %s, identity file is %s'
              % (uid, identity['uid']), file=sys.stderr)
    print('%s from drone %s, %u bytes%s'
          % (TYPES.get(ctype, 'type %u' % ctype), uid, len(plain),
             ', body not authenticated' if flags & FLAG_STREAM else ''))
    if args.out:
        open(args.out, 'wb').write(plain)


if __name__ == '__main__':
    main()
