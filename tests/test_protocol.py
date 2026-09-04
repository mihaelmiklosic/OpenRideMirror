import json
import unittest

from openridemirror_tools.protocol import (
    ActivityPacket,
    decode,
    encode_activity,
    encode_extended,
    encode_gps,
)
from openridemirror_tools.paths import repo_root


class ProtocolTests(unittest.TestCase):
    def setUp(self):
        self.golden = json.loads((repo_root() / "protocol/fixtures/golden-packets.json").read_text())

    def test_activity_golden(self):
        packet = encode_activity(ActivityPacket(42, 3, 2, 152, 123, 1_193_040, 123))
        self.assertEqual(packet.hex(), self.golden["activity"])
        self.assertEqual(decode(packet)["heart_rate"], 152)

    def test_gps_golden(self):
        packet = encode_gps(sequence=43, quality=4, latitude_e7=455_555_555,
                            longitude_e7=159_875_200, altitude_decimeters=1234,
                            heading_centidegrees=12345, timer_seconds=789)
        self.assertEqual(packet.hex(), self.golden["gps"])
        self.assertEqual(decode(packet)["longitude_e7"], 159_875_200)

    def test_extended_golden(self):
        packet = encode_extended(sequence=44, zone=4, average_hr=146, maximum_hr=152,
                                 average_speed=720, maximum_speed=950, ascent=500,
                                 calories=100, hour=14, minute=32, second=30)
        self.assertEqual(packet.hex(), self.golden["extended"])
        self.assertEqual(decode(packet)["zone"], 4)

    def test_rejects_bad_packets(self):
        with self.assertRaises(ValueError):
            decode(b"short")
        invalid = bytearray.fromhex(self.golden["gps"])
        invalid[4:8] = (999_999_999).to_bytes(4, "little", signed=True)
        with self.assertRaises(ValueError):
            decode(bytes(invalid))


if __name__ == "__main__":
    unittest.main()
