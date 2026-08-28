#!/usr/bin/env python3

# flake8: noqa

'''
read, or generate and read, the per-drone SFD identity over MAVLink

The identity operations are unsigned by design (the customer's tool holds
no key material), so this needs nothing but a link to the vehicle. It is
what the configurator does behind "SFD enable", in script form for bench
verification and for capturing an identity file at the factory.

  sfd_identity.py --device /dev/ttyACM0                  read the identity
  sfd_identity.py --device /dev/ttyACM0 --generate       generate if absent
  sfd_identity.py --device tcp:127.0.0.1:5760 --out drone.json
'''

import base64
import datetime
import json
import sys
import time

from argparse import ArgumentParser
from pymavlink import mavutil

# vendor-private SECURE_COMMAND operations, see AP_CheckFirmware.h
SECURE_COMMAND_GENERATE_IDENTITY = 0x53464401
SECURE_COMMAND_GET_IDENTITY = 0x53464402

UID_LEN = 12
KEY_LEN = 32
REPLY_TIMEOUT_S = 10

RESULT_NAMES = {
    mavutil.mavlink.MAV_RESULT_ACCEPTED: "accepted",
    mavutil.mavlink.MAV_RESULT_TEMPORARILY_REJECTED: "temporarily rejected",
    mavutil.mavlink.MAV_RESULT_DENIED: "denied",
    mavutil.mavlink.MAV_RESULT_UNSUPPORTED: "unsupported",
    mavutil.mavlink.MAV_RESULT_FAILED: "failed",
    mavutil.mavlink.MAV_RESULT_IN_PROGRESS: "in progress",
}

parser = ArgumentParser(description=__doc__.strip().splitlines()[0])
parser.add_argument("--device", required=True, help="serial device or mavutil connection string")
parser.add_argument("--baudrate", type=int, default=115200, help="serial baud rate")
parser.add_argument("--generate", action='store_true', default=False,
                    help="generate an identity if the vehicle has none")
parser.add_argument("--out", type=str, default=None, help="write the identity file here")
args = parser.parse_args()


def secure_command(conn, operation, sequence):
    '''send an unsigned SECURE_COMMAND and wait for its reply'''
    conn.mav.secure_command_send(conn.target_system, conn.target_component,
                                 sequence, operation, 0, 0, [0] * 220)
    deadline = time.time() + REPLY_TIMEOUT_S
    while time.time() < deadline:
        reply = conn.recv_match(type='SECURE_COMMAND_REPLY', blocking=True, timeout=1)
        if reply is None:
            continue
        if reply.sequence == sequence and reply.operation == operation:
            return reply
    return None


def board_id(conn):
    '''APJ_BOARD_ID from AUTOPILOT_VERSION, or None if the vehicle does not answer'''
    conn.mav.command_long_send(conn.target_system, conn.target_component,
                               mavutil.mavlink.MAV_CMD_REQUEST_MESSAGE, 0,
                               mavutil.mavlink.MAVLINK_MSG_ID_AUTOPILOT_VERSION,
                               0, 0, 0, 0, 0, 0)
    msg = conn.recv_match(type='AUTOPILOT_VERSION', blocking=True, timeout=5)
    if msg is None:
        return None
    return msg.board_version >> 16


def decode_identity(reply):
    if reply.data_length != UID_LEN + KEY_LEN:
        print("Unexpected identity length %u" % reply.data_length)
        sys.exit(1)
    data = bytes(reply.data[:reply.data_length])
    return data[:UID_LEN], data[UID_LEN:]


conn = mavutil.mavlink_connection(args.device, baud=args.baudrate)
print("Waiting for heartbeat")
conn.wait_heartbeat()
print("Connected to system %u component %u" % (conn.target_system, conn.target_component))

sequence = int(time.time()) & 0xFFFFFFFF
reply = secure_command(conn, SECURE_COMMAND_GET_IDENTITY, sequence)
if reply is None:
    print("No reply to GET_IDENTITY: firmware without signed-firmware support, or wrong link")
    sys.exit(1)

if reply.result == mavutil.mavlink.MAV_RESULT_UNSUPPORTED:
    print("Firmware does not support the identity commands")
    sys.exit(1)

if reply.result != mavutil.mavlink.MAV_RESULT_ACCEPTED:
    if not args.generate:
        print("No identity on this vehicle (%s); rerun with --generate to create one"
              % RESULT_NAMES.get(reply.result, reply.result))
        sys.exit(1)
    print("No identity yet, generating")
    sequence += 1
    reply = secure_command(conn, SECURE_COMMAND_GENERATE_IDENTITY, sequence)
    if reply is None:
        print("No reply to GENERATE_IDENTITY")
        sys.exit(1)
    if reply.result != mavutil.mavlink.MAV_RESULT_ACCEPTED:
        print("GENERATE_IDENTITY %s" % RESULT_NAMES.get(reply.result, reply.result))
        if reply.result == mavutil.mavlink.MAV_RESULT_DENIED:
            print("The vehicle is armed, or already holds an identity that GET_IDENTITY could not read")
        sys.exit(1)
    # the reply came from the key as written; read it back over a fresh
    # command as well so the file reflects what a later session will see
    sequence += 1
    check = secure_command(conn, SECURE_COMMAND_GET_IDENTITY, sequence)
    if check is None or check.result != mavutil.mavlink.MAV_RESULT_ACCEPTED:
        print("Identity generated but could not be read back")
        sys.exit(1)
    if bytes(check.data[:check.data_length]) != bytes(reply.data[:reply.data_length]):
        print("Identity read back does not match the one generated")
        sys.exit(1)
    reply = check

uid, public_key = decode_identity(reply)
identity = {
    "schema": "sfd-identity/1",
    "uid": uid.hex(),
    "public_key": base64.b64encode(public_key).decode('utf-8'),
    "board_id": board_id(conn),
    "enabled_at": datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z"),
}
print("UID        %s" % identity["uid"])
print("Public key %s" % identity["public_key"])
print("Board ID   %s" % identity["board_id"])

if args.out is not None:
    with open(args.out, "w") as f:
        json.dump(identity, f, indent=2)
        f.write("\n")
    print("Wrote %s" % args.out)
