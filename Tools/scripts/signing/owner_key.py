#!/usr/bin/env python3
'''
make an owner keypair - the key a drone encrypts its logs and settings to.

Deliberately offline and deliberately not in the configurator. The
configurator imports the private half as a non-extractable browser key and
can then use it without being able to read it, which is what lets it
decrypt at all; but a key generated *in* a browser has no backup, and
clearing site data would destroy every log the drone ever wrote. The file
this writes is that backup, and keeping it is the owner's job.

Losing it before the drone is sealed costs a re-claim. Losing it after
costs every log that drone will ever write.

  owner_key.py --out owner.json
'''

import base64
import json
from argparse import ArgumentParser
from datetime import datetime, timezone

import monocypher

# X25519 private keys reach WebCrypto as PKCS#8 and nothing else - raw
# import is refused. The wrapper is fixed for this curve, so the whole
# encoding is this prefix followed by the 32-byte scalar.
PKCS8_PREFIX = bytes.fromhex('302e020100300506032b656e04220420')


def main():
    ap = ArgumentParser(description=__doc__)
    ap.add_argument('--out', required=True, help='where to write the key file')
    args = ap.parse_args()

    private = monocypher.generate_key()
    public = monocypher.compute_key_exchange_public_key(private)

    doc = {
        'schema': 'sfd-owner/1',
        'public_key': base64.b64encode(public).decode(),
        'private_key': base64.b64encode(PKCS8_PREFIX + private).decode(),
        'created_at': datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ'),
    }
    with open(args.out, 'w') as f:
        json.dump(doc, f, indent=2)
        f.write('\n')
    print('wrote %s' % args.out)
    print('public key %s' % public.hex())
    print('')
    print('Keep this file. It is the only copy of the private half, and')
    print('nothing can regenerate it. A drone sealed against this key')
    print('writes logs that nobody without it can ever read.')


if __name__ == '__main__':
    main()
