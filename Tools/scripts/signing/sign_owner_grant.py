#!/usr/bin/env python3
'''
sign an ownership grant - SFD authorising a drone to accept an owner key.

The drone already trusts exactly one key: SFD's, in its bootloader. That
is the authorisation for claiming a drone nobody is standing next to.
This produces the artefact; the configurator relays it; the drone checks
the signature against the key it already has.

Signed offline, so the signature is over the grant alone rather than over
a session the drone issued - which is why replay is stopped by a counter
instead. Each grant for a given drone must carry a strictly larger
counter than the last one applied, or the drone refuses it. A drone that
has only ever been claimed in person has a counter of zero, so the first
grant needs at least 1.

  sign_owner_grant.py --identity drone.json --owner-key owner.json \
                      --counter 1 --key sfd_private_key.dat --out grant.bin
'''

import base64
import json
import struct
import sys
from argparse import ArgumentParser

import monocypher

MAGIC = b'SFDOWN'
VERSION = 1
GRANT_SIGNED_LEN = 60


def decode_key(ktype, key):
    '''ArduPilot's key file format, as make_secure_fw.py reads it.'''
    ktype += '_KEYV1:'
    if not key.startswith(ktype):
        sys.exit('that is not a %s key file' % ktype.rstrip('_KEYV1:'))
    return base64.b64decode(key[len(ktype):])


def main():
    ap = ArgumentParser(description=__doc__)
    ap.add_argument('--identity', required=True, help="the drone's sfd-identity/1 file")
    ap.add_argument('--owner-key', required=True, help='the sfd-owner/1 file, or a raw 32-byte public key')
    ap.add_argument('--counter', required=True, type=int, help='strictly greater than the last grant applied to this drone')
    ap.add_argument('--key', required=True, help="SFD's private signing key (the one make_secure_fw.py uses)")
    ap.add_argument('--out', required=True, help='where to write the grant')
    args = ap.parse_args()

    if args.counter < 1:
        sys.exit('counter must be at least 1: a drone claimed in person has a counter of zero')

    identity = json.load(open(args.identity))
    uid = bytes.fromhex(identity['uid'])
    if len(uid) != 12:
        sys.exit('that identity file has a %u-byte uid; expected 12' % len(uid))

    raw = open(args.owner_key, 'rb').read()
    try:
        owner_pub = base64.b64decode(json.loads(raw)['public_key'])
    except Exception:
        owner_pub = raw
    if len(owner_pub) != 32:
        sys.exit('an owner public key is 32 bytes; got %u' % len(owner_pub))

    grant = bytearray(GRANT_SIGNED_LEN)
    grant[0:6] = MAGIC
    grant[6] = VERSION
    grant[7] = 0
    grant[8:20] = uid
    grant[20:52] = owner_pub
    grant[52:60] = struct.pack('>Q', args.counter)

    # The same key file and the same decoding make_secure_fw.py uses, so
    # a drone that accepts SFD's firmware accepts SFD's grants without
    # being told anything new.
    private = decode_key('PRIVATE', open(args.key).read())
    if len(private) != 32:
        sys.exit('signing key is %u bytes; expected 32' % len(private))
    signature = monocypher.signature_sign(private, bytes(grant))

    with open(args.out, 'wb') as f:
        f.write(bytes(grant) + signature)
    print('wrote %s' % args.out)
    print('  drone     %s' % uid.hex())
    print('  owner key %s' % owner_pub.hex())
    print('  counter   %u' % args.counter)
    print('')
    print('This lets that drone be claimed by that key, and cannot be used')
    print('on any other drone or replayed after a later grant.')


if __name__ == '__main__':
    main()
