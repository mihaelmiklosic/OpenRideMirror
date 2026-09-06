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
        fixture = repo_root() / "development" / "protocol" / "fixtures" / "golden-packets.json"
        self.golden = json.loads(fixture.read_text())

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

    def test_activity_with_power_golden(self):
        packet = encode_activity(ActivityPacket(
            42, 3, 2, 152, 123, 1_193_040, 123,
            sub_sport=2, cadence_rpm=88, power_watts=172))
        self.assertEqual(packet.hex(), self.golden["activity_with_power"])
        decoded = decode(packet)
        self.assertEqual(decoded["cadence_rpm"], 88)
        self.assertEqual(decoded["power_watts"], 172)
        self.assertEqual(decoded["sub_sport"], 2)

    def test_extended_with_power_zone_golden(self):
        packet = encode_extended(sequence=44, zone=4, average_hr=146, maximum_hr=152,
                                 average_speed=666, maximum_speed=1012, ascent=500,
                                 calories=100, hour=14, minute=32, second=30,
                                 power_zone=3)
        self.assertEqual(packet.hex(), self.golden["extended_with_power_zone"])
        self.assertEqual(decode(packet)["power_zone"], 3)

    def test_new_fields_default_to_unknown(self):
        # A sender that predates these fields leaves the reserved bytes at the
        # sentinels, so a reader must see "no data" rather than a real zero.
        # Building a packet without them must produce exactly the old bytes.
        packet = encode_activity(ActivityPacket(42, 3, 2, 152, 123, 1_193_040, 123))
        self.assertEqual(packet.hex(), self.golden["activity"])
        decoded = decode(packet)
        self.assertEqual(decoded["cadence_rpm"], 0xFF)
        self.assertEqual(decoded["power_watts"], 0xFFFF)
        self.assertEqual(decoded["sub_sport"], 0xFF)

    def test_zero_power_is_not_confused_with_missing_power(self):
        # Coasting is a real reading of 0 W and must survive as 0, not as "--".
        packet = encode_activity(ActivityPacket(1, 3, 2, 150, 10, 100, 0,
                                                cadence_rpm=0, power_watts=0))
        decoded = decode(packet)
        self.assertEqual(decoded["power_watts"], 0)
        self.assertEqual(decoded["cadence_rpm"], 0)

    def test_rejects_invalid_power_zone(self):
        packet = bytearray(encode_extended(
            sequence=1, zone=3, average_hr=140, maximum_hr=150, average_speed=600,
            maximum_speed=900, ascent=100, calories=50, hour=10, minute=0, second=0,
            power_zone=3))
        packet[17] = 9  # Coggan tops out at 7; 8..254 are not valid zones.
        with self.assertRaises(ValueError):
            decode(bytes(packet))

    def test_rejects_bad_packets(self):
        with self.assertRaises(ValueError):
            decode(b"short")
        invalid = bytearray.fromhex(self.golden["gps"])
        invalid[4:8] = (999_999_999).to_bytes(4, "little", signed=True)
        with self.assertRaises(ValueError):
            decode(bytes(invalid))


if __name__ == "__main__":
    unittest.main()
