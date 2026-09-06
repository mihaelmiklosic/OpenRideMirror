"""Comprueba que el firmware ESP32 lea cada campo del offset que fija el schema.

The ESP32 parses telemetry with hand-written offsets:

    liveData.speedCentimetersPerSecond = readU16(bytes, 16);

Nothing tied those numbers to development/protocol/orm-protocol.json, which is
the declared source of truth. A field read one byte off, or with the wrong
width, produces a display full of plausible-looking nonsense -- and the mistake
only shows up with a board on a handlebar, which is the worst place to debug it.

Parsing C++ with regular expressions is normally a bad idea. It is acceptable
here because the check is narrow and self-limiting: every accessor the firmware
uses is matched by name, and the test fails if a packet ends up with no
recognised reads at all, so a parsing change cannot silently turn this into a
test that verifies nothing.
"""
from __future__ import annotations

import json
import re
import unittest

from openridemirror_tools.paths import repo_root

FIRMWARE = (repo_root() / "firmware" / "esp32" / "OpenRideMirror"
            / "OpenRideMirror.ino")
SCHEMA = repo_root() / "development" / "protocol" / "orm-protocol.json"

# Firmware accessor -> the schema formats it is valid for.
ACCESSOR_FORMATS = {
    "readU16": {"u16"},
    "readS16": {"s16"},
    "readU32": {"u32"},
    "readS32": {"s32"},
}

# Firmware field name -> schema field name. Only fields the firmware actually
# consumes; reserved bytes are deliberately absent.
FIELD_ALIASES = {
    "timerSeconds": "timer_seconds",
    "distanceDecimeters": "distance_decimeters",
    "speedCentimetersPerSecond": "speed_centimeters_per_second",
    "latitudeE7": "latitude_e7",
    "longitudeE7": "longitude_e7",
    "altitudeDecimeters": "altitude_decimeters",
    "headingCentidegrees": "heading_centidegrees",
    "gpsTimerSeconds": "timer_seconds",
    "averageSpeedCentimetersPerSecond": "average_speed_centimeters_per_second",
    "maxSpeedCentimetersPerSecond": "maximum_speed_centimeters_per_second",
    "totalAscentDecimeters": "total_ascent_decimeters",
    "calories": "calories_kcal",
    "sequence": "sequence",
    "timerState": "timer_state",
    "sport": "sport",
    "subSport": "sub_sport",
    "heartRate": "heart_rate_bpm",
    "gpsQuality": "gps_quality",
    "heartRateZone": "heart_rate_zone",
    "averageHeartRate": "average_heart_rate_bpm",
    "maxHeartRate": "maximum_heart_rate_bpm",
    "clockHour": "local_hour",
    "clockMinute": "local_minute",
    "clockSecond": "local_second",
}


def schema_layout() -> dict[str, dict[str, tuple[int, str]]]:
    """{packet name: {field name: (offset, format)}}"""
    schema = json.loads(SCHEMA.read_text())
    return {
        packet["name"]: {
            field["name"]: (field["offset"], field["format"])
            for field in packet["fields"]
        }
        for packet in schema["packets"]
    }


def firmware_reads() -> dict[str, list[tuple[str, int, str]]]:
    """Field reads in the firmware, grouped by the packet branch they sit in.

    Returns {packet: [(field, offset, format), ...]}.
    """
    source = FIRMWARE.read_text()
    # Anchored on the exact if/else-if chain inside the packet handler. Looser
    # patterns matched an earlier, unrelated use of PACKET_GPS elsewhere in the
    # file and attributed activity offsets to the GPS branch.
    branches = {
        "activity": (r"if \(bytes\[0\] == PACKET_ACTIVITY\) \{"
                     r"(.*?)\} else if \(bytes\[0\] == PACKET_GPS\) \{"),
        "gps": (r"\} else if \(bytes\[0\] == PACKET_GPS\) \{"
                r"(.*?)\} else if \(bytes\[0\] == PACKET_EXTENDED\) \{"),
        "extended": (r"\} else if \(bytes\[0\] == PACKET_EXTENDED\) \{"
                     r"(.*?)\} else \{"),
    }
    reads: dict[str, list[tuple[str, int, str]]] = {}
    for packet, pattern in branches.items():
        match = re.search(pattern, source, re.DOTALL)
        if match is None:
            raise AssertionError(f"could not locate the {packet} branch in the firmware")
        body = match.group(1)
        found = []
        # Multi-byte reads: liveData.field = readU16(bytes, 16);
        for field, accessor, offset in re.findall(
                r"liveData\.(\w+)\s*=\s*(readU16|readS16|readU32|readS32)\(bytes,\s*(\d+)\)",
                body):
            found.append((field, int(offset), accessor))
        # Single-byte reads: liveData.field = bytes[4];
        for field, offset in re.findall(r"liveData\.(\w+)\s*=\s*bytes\[(\d+)\]", body):
            found.append((field, int(offset), "u8"))
        reads[packet] = found
    return reads


class FirmwareOffsetTests(unittest.TestCase):
    def setUp(self) -> None:
        self.layout = schema_layout()
        self.reads = firmware_reads()

    def test_every_packet_branch_reads_something(self) -> None:
        # Guards the regex above: if the firmware is restructured and the
        # patterns stop matching real reads, this fails instead of passing
        # vacuously.
        for packet, found in self.reads.items():
            self.assertGreater(len(found), 0, f"no field reads found for {packet}")

    def test_firmware_offsets_match_the_schema(self) -> None:
        for packet, found in self.reads.items():
            fields = self.layout[packet]
            for firmware_field, offset, accessor in found:
                schema_field = FIELD_ALIASES.get(firmware_field)
                if schema_field is None:
                    continue  # derived state, not a wire field
                self.assertIn(schema_field, fields,
                              f"{packet}.{schema_field} is not in the schema")
                expected_offset, expected_format = fields[schema_field]
                self.assertEqual(
                    offset, expected_offset,
                    f"{packet}.{firmware_field} reads offset {offset}, "
                    f"schema says {expected_offset}")
                if accessor in ACCESSOR_FORMATS:
                    self.assertIn(
                        expected_format, ACCESSOR_FORMATS[accessor],
                        f"{packet}.{firmware_field} uses {accessor} for a "
                        f"{expected_format} field")
                else:
                    self.assertEqual(
                        expected_format, "u8",
                        f"{packet}.{firmware_field} reads a single byte but the "
                        f"schema says {expected_format}")

    def test_firmware_does_not_read_reserved_bytes(self) -> None:
        # Reserved positions carry no meaning in v1; reading one means the
        # firmware is showing a value the protocol never promised.
        schema = json.loads(SCHEMA.read_text())
        reserved = {
            packet["name"]: {field["offset"] for field in packet["fields"]
                             if field.get("reserved")}
            for packet in schema["packets"]
        }
        for packet, found in self.reads.items():
            for firmware_field, offset, _ in found:
                if FIELD_ALIASES.get(firmware_field) in (None, "sub_sport"):
                    continue
                self.assertNotIn(
                    offset, reserved[packet],
                    f"{packet}.{firmware_field} reads reserved offset {offset}")


if __name__ == "__main__":
    unittest.main()
