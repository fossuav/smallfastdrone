#!/usr/bin/env python3
'''
encrypt a lua script for one specific drone, using its identity

Writes a .lxa v2 file. v1 encrypted with a bootloader *public* key used as
a symmetric key, which is obfuscation rather than encryption: that key is
in every bootloader, so anyone with the firmware could read the script.

v2 agrees a key with the drone's own identity instead. An ephemeral key
pair is generated here, X25519'd against the drone's identity public key,
and only the public half is written to the file. The drone re-derives the
same secret from its identity private key, which never leaves the chip -
so the script can be read by that airframe and no other.

The drone's identity comes from the sfd-identity/1 file the configurator
produces when a drone is enabled.

Layout, matching AP_Scripting_config.h:

     0    6   magic "LXA2.0"
     6   12   target board id
    18   32   sender's ephemeral X25519 public key
    50   24   nonce
    74   16   Poly1305 tag
    90  ...   XChaCha20-Poly1305 ciphertext
'''

import base64
import json
import os
import secrets
import sys
from argparse import ArgumentParser

MAGIC = b"LXA2.0"
UID_LEN = 12
NONCE_LEN = 24

parser = ArgumentParser(description='Lua encryption utility')
parser.add_argument("script", type=str, help="script to encrypt")
parser.add_argument("identity", type=str,
                    help="the drone's sfd-identity/1 JSON file, as saved by the configurator")
parser.add_argument("-o", "--output", type=str, default=None,
                    help="output file (default: alongside the script, as .lxa)")
args = parser.parse_args()

try:
    import monocypher
except ImportError:
    print("Please install monocypher with: python3 -m pip install pymonocypher==3.1.3.2")
    sys.exit(1)

if monocypher.__version__ != "3.1.3.2":
    print("must use monocypher 3.1.3.2, please run: python3 -m pip install pymonocypher==3.1.3.2")
    sys.exit(1)


def load_identity(path):
    '''read the drone's uid and public key out of an sfd-identity/1 file'''
    with open(path, "r") as f:
        doc = json.load(f)
    schema = doc.get("schema")
    if schema != "sfd-identity/1":
        print("%s is not an sfd-identity/1 file (schema=%s)" % (path, schema))
        sys.exit(1)
    uid = bytes.fromhex(doc["uid"])
    if len(uid) != UID_LEN:
        print("identity uid is %u bytes, expected %u" % (len(uid), UID_LEN))
        sys.exit(1)
    public_key = base64.b64decode(doc["public_key"])
    if len(public_key) != 32:
        print("identity public key is %u bytes, expected 32" % len(public_key))
        sys.exit(1)
    return uid, public_key


uid, drone_public = load_identity(args.identity)

with open(args.script, "rb") as f:
    msg = f.read()

# ephemeral key pair: its private half exists only for this file, and only
# here. Losing it costs nothing, which is the point of it being ephemeral.
ephemeral_private = monocypher.generate_key()
ephemeral_public = monocypher.compute_key_exchange_public_key(ephemeral_private)
shared = monocypher.key_exchange(ephemeral_private, drone_public)

nonce = secrets.token_bytes(NONCE_LEN)
mac, ciphertext = monocypher.lock(shared, nonce, msg)

out = args.output or (os.path.splitext(args.script)[0] + ".lxa")
with open(out, "wb") as f:
    f.write(MAGIC)
    f.write(uid)
    f.write(ephemeral_public)
    f.write(nonce)
    f.write(mac)
    f.write(ciphertext)

print("wrote %s (%u bytes) for drone %s" % (out, os.path.getsize(out), uid.hex()))
