# SPDX-License-Identifier: GPL-3.0-or-later
"""Binary wire fixtures and state tests independent of the decoder's structs."""

import json
import math
from pathlib import Path
import socket
import struct
import sys
import unittest

TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))
import openseeface_adapter as bridge
from send_demo import demo_values


def packet(face_id=0, timestamp=1700000000.0, euler=(180.0, 0.0, 90.0),
           success=1, right_eye=0.2, left_eye=0.9, mouth_open=0.6):
    # Build by documented byte offsets, independent of HEADER/TAIL format.
    data = bytearray(1785)
    struct.pack_into("<d", data, 0, timestamp)
    struct.pack_into("<i", data, 8, face_id)
    struct.pack_into("<4f", data, 12, 640, 480, right_eye, left_eye)
    data[28] = success
    struct.pack_into("<f", data, 29, 3.0)
    struct.pack_into("<4f", data, 33, 0, 0, 0, 1)
    struct.pack_into("<3f", data, 49, *euler)
    for index in range(68):
        struct.pack_into("<f", data, 73 + index * 4, 0.8 if index < 66 else 0.0)
    # Tail feature locations: corners are negative when raised in this model.
    struct.pack_into("<f", data, 1729 + 8 * 4, -0.4)
    struct.pack_into("<f", data, 1729 + 10 * 4, -0.8)
    struct.pack_into("<f", data, 1729 + 12 * 4, mouth_open)
    return bytes(data)


class AdapterTests(unittest.TestCase):
    def test_neutral_pose_and_anatomical_eye_sides(self):
        face = bridge.decode_datagram(packet())[0]
        values = bridge.face_values(face)
        self.assertTrue(values["tracking"])
        self.assertEqual(values["head"], {"yaw": 0.0, "pitch": 0.0, "roll": 0.0})
        self.assertAlmostEqual(values["eyes"]["right"], 0.2)
        self.assertAlmostEqual(values["eyes"]["left"], 0.9)
        self.assertAlmostEqual(values["confidence"], 0.8)
        self.assertAlmostEqual(values["mouth"]["open"], 0.6)
        self.assertAlmostEqual(values["mouth"]["smile"], 0.6)

    def test_wrap_at_pitch_seam_and_pose_units(self):
        values = bridge.face_values(bridge.decode_datagram(
            packet(euler=(-175, 30, 100)))[0])
        self.assertAlmostEqual(values["head"]["pitch"], math.radians(-5))
        self.assertAlmostEqual(values["head"]["yaw"], math.radians(-30))
        self.assertAlmostEqual(values["head"]["roll"], math.radians(10))

    def test_multiple_faces_and_latest_queue(self):
        state = bridge.LatestFace(face_id=7)
        state.ingest(packet(3) + packet(7, 20, mouth_open=0.1), "127.0.0.1", 10.0)
        state.ingest(packet(7, 21, mouth_open=0.8), "127.0.0.1", 10.1)
        self.assertAlmostEqual(state.poll(10.2)["mouth"]["open"], 0.8)
        self.assertIsNone(state.poll(10.3))
        # Old source packets do not extend timeout or overwrite motion.
        self.assertFalse(state.ingest(packet(7, 19), "127.0.0.1", 10.4))
        self.assertFalse(state.ingest(packet(3, 30), "127.0.0.1", 10.5))
        self.assertFalse(state.poll(10.95)["tracking"])
        self.assertIsNone(state.poll(11.0))
        self.assertTrue(state.ingest(packet(7, 1), "127.0.0.1", 11.1))
        self.assertTrue(state.poll(11.2)["tracking"])

    def test_remote_sender_does_not_change_state(self):
        state = bridge.LatestFace()
        self.assertFalse(state.ingest(packet(), "192.168.1.2", 10.0))
        self.assertIsNone(state.last_seen)

    def test_no_3d_pose_returns_neutral(self):
        values = bridge.face_values(bridge.decode_datagram(packet(success=0))[0])
        self.assertEqual(values, bridge.neutral_values())

    def test_malformed_datagrams(self):
        for data in (b"", packet()[:-1], packet() + b"x", packet() * 37):
            with self.subTest(length=len(data)), self.assertRaises(bridge.PacketError):
                bridge.decode_datagram(data)
        for offset, value in ((49, float("nan")), (889, float("inf")),
                              (1729, float("nan")), (73, 2.0), (12, -1.0)):
            data = bytearray(packet())
            struct.pack_into("<f", data, offset, value)
            with self.subTest(offset=offset), self.assertRaises(bridge.PacketError):
                bridge.decode_datagram(bytes(data))

    def test_demo_has_blink_and_valid_expression_ranges(self):
        self.assertAlmostEqual(demo_values(0.15)["eyes"]["left"], 0.0)
        self.assertEqual(demo_values(1.0)["eyes"]["left"], 1.0)
        for index in range(1000):
            values = demo_values(index / 30.0)
            for group in ("eyes", "mouth"):
                self.assertTrue(all(0 <= value <= 1 for value in values[group].values()))
            self.assertTrue(all(-math.pi <= value <= math.pi
                                for value in values["head"].values()))

    def test_actual_udp_envelope_and_sequence_wrap(self):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as receiver:
            receiver.bind(("127.0.0.1", 0))
            receiver.settimeout(1.0)
            sender = bridge.SF1Sender("127.0.0.1", receiver.getsockname()[1], "a" * 32)
            try:
                sender.seq = 0xFFFFFFFF
                sender.send(demo_values(1.0))
                sender.send(bridge.neutral_values())
                first = json.loads(receiver.recv(4096))
                second = json.loads(receiver.recv(4096))
                self.assertEqual((first["v"], first["type"], first["token"]),
                                 (1, "frame", "a" * 32))
                self.assertEqual((first["seq"], second["seq"]), (0xFFFFFFFF, 0))
                self.assertTrue(first["tracking"])
                self.assertFalse(second["tracking"])
            finally:
                sender.close()


if __name__ == "__main__":
    unittest.main()
