#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 samiosu
# SPDX-License-Identifier: GPL-3.0-or-later
"""Bridge a separately installed OpenSeeFace tracker to SpriteFace SF2 UDP.

This file does not run face recognition and contains no OpenSeeFace source or
model weights. Its independently written decoder follows the upstream packet
layout in facetracker.py and the coordinate conventions in Unity/OpenSee.cs:
https://github.com/emilianavt/OpenSeeFace/blob/50c09741897010faad721a218b2bbe14ecad6251/facetracker.py
https://github.com/emilianavt/OpenSeeFace/blob/50c09741897010faad721a218b2bbe14ecad6251/Unity/OpenSee.cs
Protocol inspected 2026-10-13: 68 landmarks, 70 3D points, 14 features,
1,785 bytes per face. Upstream sends packed native-endian fields; this bridge
supports the little-endian packet used on x86/ARM Windows, macOS, and Linux.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
import math
import re
import select
import socket
import struct
import sys
import time
from typing import Any
from cryptography.exceptions import InvalidTag

from secure_packet import challenge, open_packet, seal, valid_challenge


RECORD_SIZE = 1785
MAX_DATAGRAM = 65507
HEADER = struct.Struct("<di4fB11f")
TAIL = struct.Struct("<428f")  # 68 confidence + 136 xy + 210 xyz + 14 features
FEATURE_NAMES = (
    "eye_l", "eye_r", "eyebrow_steepness_l", "eyebrow_updown_l",
    "eyebrow_quirk_l", "eyebrow_steepness_r", "eyebrow_updown_r",
    "eyebrow_quirk_r", "mouth_corner_updown_l", "mouth_corner_inout_l",
    "mouth_corner_updown_r", "mouth_corner_inout_r", "mouth_open", "mouth_wide",
)


class PacketError(ValueError):
    """The binary datagram is truncated, nonfinite, or outside sane bounds."""


@dataclass(frozen=True)
class Face:
    timestamp: float
    face_id: int
    tracking: bool
    confidence: float
    right_eye: float
    left_eye: float
    euler: tuple[float, float, float]
    features: dict[str, float]


def clamp01(value: float) -> float:
    return max(0.0, min(1.0, value))


def wrap_radians(degrees: float) -> float:
    """Center angles at zero, preserving a small motion across the 180 seam."""
    return math.radians((degrees + 180.0) % 360.0 - 180.0)


def decode_datagram(data: bytes) -> list[Face]:
    """Decode a complete datagram, which can contain multiple face records.

    Rejecting the entire malformed datagram avoids using a partially received
    record or accepting NaN values even in fields not used by the bridge.
    """
    if not data or len(data) > MAX_DATAGRAM or len(data) % RECORD_SIZE:
        raise PacketError("expected complete 1,785-byte face records")
    faces = []
    for offset in range(0, len(data), RECORD_SIZE):
        header = HEADER.unpack_from(data, offset)
        timestamp, face_id, width, height, right_eye, left_eye, success = header[:7]
        fit_error = header[7]
        quaternion = header[8:12]
        euler = header[12:15]
        translation = header[15:18]
        tail = TAIL.unpack_from(data, offset + HEADER.size)
        floats = (timestamp, *header[2:6], *header[7:], *tail)
        if not all(math.isfinite(value) for value in floats):
            raise PacketError("nonfinite tracking value")
        if timestamp < 0 or face_id < 0 or success not in (0, 1):
            raise PacketError("invalid timestamp, face id, or 3D success flag")
        if not (0 < width <= 16384 and 0 < height <= 16384):
            raise PacketError("invalid camera resolution")
        if not (0 <= right_eye <= 1 and 0 <= left_eye <= 1):
            raise PacketError("invalid eye openness")
        if not 0 <= fit_error <= 1_000_000:
            raise PacketError("invalid 3D fitting error")
        if any(abs(value) > 1.01 for value in quaternion):
            raise PacketError("invalid quaternion")
        if any(abs(value) > 360 for value in euler):
            raise PacketError("invalid Euler angle")
        if any(abs(value) > 10_000_000 for value in translation):
            raise PacketError("invalid translation")
        confidences = tail[:68]
        landmarks = tail[68:204]
        points3d = tail[204:414]
        feature_values = tail[414:]
        if any(not 0 <= value <= 1 for value in confidences):
            raise PacketError("invalid landmark confidence")
        if any(abs(value) > 10 * max(width, height) for value in landmarks):
            raise PacketError("invalid landmark position")
        if any(abs(value) > 10_000_000 for value in points3d):
            raise PacketError("invalid 3D point")
        if any(abs(value) > 2 for value in feature_values):
            raise PacketError("invalid expression feature")
        # The sender does not include its aggregate confidence. Ignore the last
        # two pupil points so blinking does not decrease face confidence.
        confidence = sum(confidences[:66]) / 66.0
        faces.append(Face(timestamp, face_id, bool(success), confidence,
                          right_eye, left_eye, tuple(euler),
                          dict(zip(FEATURE_NAMES, feature_values))))
    return faces


def neutral_values() -> dict[str, Any]:
    return {"tracking": False, "confidence": 0.0,
            "head": {"yaw": 0.0, "pitch": 0.0, "roll": 0.0},
            "eyes": {"left": 1.0, "right": 1.0},
            "mouth": {"open": 0.0, "smile": 0.0}}


def face_values(face: Face, min_confidence: float = 0.2) -> dict[str, Any]:
    if not face.tracking or face.confidence < min_confidence:
        return neutral_values()
    ex, ey, ez = face.euler
    # OpenSeeFace's neutral pose is X +/-180 and Z 90 degrees. Signs and
    # offsets follow its receiver's rotation convention, now in radians.
    head = {"yaw": wrap_radians(-ey), "pitch": wrap_radians(-(ex + 180.0)),
            "roll": wrap_radians(ez - 90.0)}
    # Features are calibrated relative to a running median, not ARKit
    # blendshapes. Positive mouth_open gives opening above that median.
    smile = -(face.features["mouth_corner_updown_l"]
              + face.features["mouth_corner_updown_r"]) / 2.0
    return {"tracking": True, "confidence": clamp01(face.confidence),
            "head": head, "eyes": {"left": face.left_eye, "right": face.right_eye},
            "mouth": {"open": clamp01(face.features["mouth_open"]),
                      "smile": clamp01(smile)}}


class LatestFace:
    """One pending value, timestamp ordering, and a single loss notification."""

    def __init__(self, face_id: int = 0, timeout: float = 0.8,
                 min_confidence: float = 0.2):
        self.face_id = face_id
        self.timeout = timeout
        self.min_confidence = min_confidence
        self.pending: Face | None = None
        self.last_seen: float | None = None
        self.last_timestamp: float | None = None
        self.loss_sent = False

    def ingest(self, data: bytes, sender_host: str, now: float) -> bool:
        # OpenSeeFace has no token authentication: only local input is accepted.
        if sender_host != "127.0.0.1":
            return False
        faces = decode_datagram(data)
        selected = [face for face in faces if face.face_id == self.face_id]
        if not selected:
            return False
        newest = max(selected, key=lambda face: face.timestamp)
        if self.last_timestamp is not None and newest.timestamp <= self.last_timestamp:
            return False
        self.pending = newest
        self.last_seen = now
        self.last_timestamp = newest.timestamp
        self.loss_sent = False
        return True

    def poll(self, now: float) -> dict[str, Any] | None:
        if self.last_seen is None or now - self.last_seen >= self.timeout:
            self.pending = None
            if not self.loss_sent:
                self.loss_sent = True
                # Allow a restarted tracker (or corrected system clock) to
                # recover even if its next source timestamp is smaller.
                self.last_timestamp = None
                return neutral_values()
            return None
        if self.pending is None:
            return None
        face, self.pending = self.pending, None
        return face_values(face, self.min_confidence)


class SF2Sender:
    """Authenticated sender shared by the adapter and synthetic demo."""

    def __init__(self, host: str, port: int, token: str):
        self.destination = (socket.gethostbyname(host), port)
        self.token = token
        self.seq = 0
        self.client = challenge()
        self.server: str | None = None
        self.next_hello = 0.0
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.connect(self.destination)
        self.socket.setblocking(False)

    def send(self, values: dict[str, Any] | None) -> bool:
        for _ in range(32):
            try:
                response = json.loads(open_packet(self.socket.recv(2048), self.token))
            except BlockingIOError:
                break
            except (ConnectionRefusedError, ConnectionResetError):
                # Connected UDP reports an absent peer through recv on some OSes.
                # Keep the sender alive so the next hello can pair after restart.
                self.server = None
                break
            except (ValueError, UnicodeError, InvalidTag, AttributeError, TypeError):
                continue
            if response.get("v") != 1 or response.get("client") != self.client:
                continue
            if response.get("type") == "ready" and valid_challenge(response.get("server", "")):
                if self.server != response["server"]:
                    self.seq = 0
                self.server = response["server"]
            elif response.get("type") == "busy":
                self.server = None
        now = time.monotonic()
        if now >= self.next_hello:
            hello = {"v": 1, "type": "hello", "token": self.token, "client": self.client}
            self.socket.send(seal(json.dumps(hello, separators=(",", ":")).encode(), self.token))
            self.next_hello = now + 1.0
        if self.server is None or values is None:
            return False
        frame = {"v": 1, "type": "frame", "token": self.token, "client": self.client,
                 "server": self.server, "seq": self.seq,
                 **values}
        encoded = json.dumps(frame, separators=(",", ":"), allow_nan=False).encode("utf-8")
        self.socket.send(seal(encoded, self.token))
        self.seq = (self.seq + 1) & 0xFFFFFFFF
        return True

    def close(self) -> None:
        self.socket.close()


def token_argument(value: str) -> str:
    if not re.fullmatch(r"[0-9a-fA-F]{32}", value):
        raise argparse.ArgumentTypeError("token must be exactly 32 hexadecimal characters")
    return value.lower()


def port_argument(value: str) -> int:
    try:
        port = int(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("port must be an integer") from error
    if not 1 <= port <= 65535:
        raise argparse.ArgumentTypeError("port must be between 1 and 65535")
    return port


def finite_argument(value: str) -> float:
    try:
        number = float(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("expected a finite number") from error
    if not math.isfinite(number):
        raise argparse.ArgumentTypeError("expected a finite number")
    return number


def add_sender_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--token", required=True, type=token_argument,
                        help="SpriteFace's 32-hex-character pairing token")
    parser.add_argument("--host", default="127.0.0.1", help="SpriteFace receiver host")
    parser.add_argument("--port", type=port_argument, default=11574,
                        help="SpriteFace SF2 UDP port (default: 11574)")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    add_sender_arguments(parser)
    parser.add_argument("--listen-port", type=port_argument, default=11573,
                        help="OpenSeeFace input UDP port on 127.0.0.1 (default: 11573)")
    parser.add_argument("--face-id", type=int, default=0, help="OpenSeeFace face ID (default: 0)")
    parser.add_argument("--fps", type=finite_argument, default=60,
                        help="maximum forwarding rate (default: 60)")
    parser.add_argument("--timeout", type=finite_argument, default=0.8,
                        help="seconds without a valid face before neutral (default: 0.8)")
    parser.add_argument("--min-confidence", type=finite_argument, default=0.2,
                        help="minimum mean landmark confidence (default: 0.2)")
    args = parser.parse_args(argv)
    if args.face_id < 0 or not 1 <= args.fps <= 240 or not 0.1 <= args.timeout <= 10:
        parser.error("face-id must be nonnegative; fps must be 1..240; timeout must be 0.1..10")
    if not 0 <= args.min_confidence <= 1:
        parser.error("min-confidence must be between 0 and 1")
    receiver = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sender = None
    try:
        receiver.bind(("127.0.0.1", args.listen_port))
        receiver.setblocking(False)
        sender = SF2Sender(args.host, args.port, args.token)
        state = LatestFace(args.face_id, args.timeout, args.min_confidence)
        print(f"OpenSeeFace bridge: 127.0.0.1:{args.listen_port} -> "
              f"{args.host}:{args.port}; face ID {args.face_id}", flush=True)
        interval = 1.0 / args.fps
        next_send = time.monotonic()
        while True:
            wait = max(0.0, min(0.05, next_send - time.monotonic()))
            readable, _, _ = select.select([receiver], [], [], wait)
            if readable:
                # A bounded drain keeps a busy sender from delaying timeout
                # and publication forever. No growing frame queue is kept.
                for _ in range(256):
                    try:
                        data, source = receiver.recvfrom(MAX_DATAGRAM + 1)
                    except BlockingIOError:
                        break
                    try:
                        state.ingest(data, source[0], time.monotonic())
                    except PacketError:
                        pass
            now = time.monotonic()
            if now >= next_send:
                values = state.poll(now)
                sender.send(values)
                next_send = now + interval
    except KeyboardInterrupt:
        if sender is not None:
            sender.send(neutral_values())
        return 0
    except OSError as error:
        print(f"OpenSeeFace bridge: {error}", file=sys.stderr)
        return 1
    finally:
        receiver.close()
        if sender is not None:
            sender.close()


if __name__ == "__main__":
    raise SystemExit(main())
