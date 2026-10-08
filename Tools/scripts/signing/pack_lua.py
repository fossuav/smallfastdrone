#!/usr/bin/env python3
'''
pack a lua script product for one specific drone

A product is usually several files - applets that live in APM/scripts and
the modules they require() from APM/scripts/modules. On a signed build every
one of them has to be encrypted for the drone that runs it, public helpers
such as crsf_helper included, because the module searcher passes every
module through the decryptor whether it is encrypted or not. This encrypts
them all for one drone and writes a single sfd-bundle/1 file, which the
configurator installs in one go.

The manifest names each source file and where it lands on the drone:

    {
      "product": "acro_fence",
      "title": "Acro fence",
      "files": [
        {"src": "acro_fence.lua", "dest": "scripts/acro_fence.lua"},
        {"src": "ardupilot:libraries/AP_Scripting/modules/crsf_helper.lua",
         "dest": "scripts/modules/crsf_helper.lua"}
      ]
    }

src is relative to the manifest. An "ardupilot:" prefix makes it relative
to the source tree this script is in, which is where public helpers come
from. dest is relative to APM/ and names the plaintext file; the bundle
carries it as .lxa.

The bundle is JSON, the same shape as an .apj:

    {
      "schema": "sfd-bundle/1",
      "product": "acro_fence",
      "title": "Acro fence",
      "version": "ap_lua 3f2a9c1",
      "uid": "<the drone's 12-byte uid, lower-case hex>",
      "files": [{"path": "scripts/acro_fence.lxa", "data": "<base64>"}]
    }

Nothing in it is secret. Each file is a .lxa v2 that only the drone with
that uid can open, and the header is only an address and a packing list.
'''

import base64
import json
import os
import re
import subprocess
import sys
from argparse import ArgumentParser

from encrypt_lua import encrypt, load_identity

SCHEMA = "sfd-bundle/1"
PRODUCT_RE = re.compile(r"^[A-Za-z0-9_]+$")
# the two places the firmware loads from: top-level scripts, and the
# modules/?.lxa entry on the require() path
DEST_RE = re.compile(r"^scripts/(modules/)?[A-Za-z0-9_]+\.lua$")
ARDUPILOT_PREFIX = "ardupilot:"
ARDUPILOT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))


def fail(msg):
    '''stop with a message rather than a traceback'''
    print(msg)
    sys.exit(1)


def load_manifest(path):
    '''read and check a product manifest; return (product, title, files)'''
    with open(path, "r") as f:
        doc = json.load(f)
    product = doc.get("product")
    if not isinstance(product, str) or not PRODUCT_RE.match(product):
        fail("%s: product must be letters, digits and _ (got %r)" % (path, product))
    title = doc.get("title")
    if not isinstance(title, str) or not title.strip():
        fail("%s: title must be a non-empty string" % path)
    files = doc.get("files")
    if not isinstance(files, list) or len(files) == 0:
        fail("%s: files must list at least one script" % path)

    base = os.path.dirname(os.path.abspath(path))
    seen = set()
    out = []
    for entry in files:
        src = entry.get("src") if isinstance(entry, dict) else None
        dest = entry.get("dest") if isinstance(entry, dict) else None
        if not isinstance(src, str) or not isinstance(dest, str):
            fail("%s: every file needs a src and a dest" % path)
        if not DEST_RE.match(dest):
            fail("%s: %s must be scripts/<name>.lua or scripts/modules/<name>.lua" % (path, dest))
        if dest in seen:
            fail("%s: %s is listed twice" % (path, dest))
        seen.add(dest)
        if src.startswith(ARDUPILOT_PREFIX):
            src_path = os.path.join(ARDUPILOT_ROOT, src[len(ARDUPILOT_PREFIX):])
        else:
            src_path = os.path.join(base, src)
        if not os.path.isfile(src_path):
            fail("%s: %s not found (%s)" % (path, src, src_path))
        out.append((src_path, dest))
    return product, title.strip(), out


def describe_version(manifest_path):
    '''the manifest repo's name and git describe, or None outside git'''
    base = os.path.dirname(os.path.abspath(manifest_path))
    try:
        top = subprocess.check_output(["git", "-C", base, "rev-parse", "--show-toplevel"],
                                      stderr=subprocess.DEVNULL, text=True).strip()
        desc = subprocess.check_output(["git", "-C", base, "describe", "--always", "--dirty"],
                                       stderr=subprocess.DEVNULL, text=True).strip()
    except (OSError, subprocess.CalledProcessError):
        return None
    return "%s %s" % (os.path.basename(top), desc)


def main():
    parser = ArgumentParser(description='pack a lua script product for one drone')
    parser.add_argument("manifest", type=str, help="the product manifest (JSON)")
    parser.add_argument("identity", type=str,
                        help="the drone's sfd-identity/1 JSON file, as saved by the configurator")
    parser.add_argument("-o", "--output", type=str, default=None,
                        help="output file (default: <product>-<uid>.sfdbundle in the current directory)")
    parser.add_argument("--version", type=str, default=None,
                        help="version string to record (default: git describe of the manifest's repo)")
    args = parser.parse_args()

    product, title, files = load_manifest(args.manifest)
    uid, drone_public = load_identity(args.identity)
    version = args.version or describe_version(args.manifest)

    bundle = {"schema": SCHEMA, "product": product, "title": title}
    if version is not None:
        bundle["version"] = version
    bundle["uid"] = uid.hex()
    bundle["files"] = []
    for src_path, dest in files:
        with open(src_path, "rb") as f:
            msg = f.read()
        lxa = encrypt(msg, uid, drone_public)
        bundle["files"].append({"path": dest[:-len(".lua")] + ".lxa",
                                "data": base64.b64encode(lxa).decode("ascii")})
        print("  %-40s %7u bytes" % (dest, len(msg)))

    out = args.output or "%s-%s.sfdbundle" % (product, uid.hex())
    with open(out, "w") as f:
        json.dump(bundle, f, indent=2)
        f.write("\n")

    print("wrote %s: %s, %u file(s) for drone %s" % (out, title, len(files), uid.hex()))


if __name__ == '__main__':
    main()
