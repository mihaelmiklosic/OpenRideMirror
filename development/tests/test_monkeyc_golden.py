"""Keeps the Monkey C encoder tests tied to the shared golden fixtures.

test/OrmProtocolTest.mc rebuilds the activity packet and compares it against a
byte literal, because Monkey C running on the simulator cannot read a JSON file
off disk. That literal is therefore a copy of
development/protocol/fixtures/golden-packets.json, and a copy can drift.

This test fails if it ever does, which is what makes the duplicate safe: the
fixture stays the single source of truth even though one consumer holds a copy.
"""
from __future__ import annotations

import json
import re
import unittest

from openridemirror_tools.paths import repo_root

MONKEYC_TEST = (repo_root() / "garmin" / "OpenRideMirror" / "test"
                / "OrmProtocolTest.mc")
FIXTURES = (repo_root() / "development" / "protocol" / "fixtures"
            / "golden-packets.json")


def monkeyc_byte_literal(name: str) -> bytes:
    """Read a `const NAME = [ 0x.., ... ]b;` literal out of the Monkey C source."""
    source = MONKEYC_TEST.read_text()
    match = re.search(rf"const\s+{re.escape(name)}\s*=\s*\[(.*?)\]b\s*;",
                      source, re.DOTALL)
    if match is None:
        raise AssertionError(f"{name} not found in {MONKEYC_TEST.name}")
    return bytes(int(token, 16) for token in re.findall(r"0x([0-9a-fA-F]{2})",
                                                        match.group(1)))


# Monkey C literal -> fixture key.
MIRRORED = {
    "GOLDEN_ACTIVITY": "activity",
    "GOLDEN_ACTIVITY_WITH_POWER": "activity_with_power",
}


class MonkeyCGoldenTests(unittest.TestCase):
    def test_literals_match_the_fixtures(self) -> None:
        fixtures = json.loads(FIXTURES.read_text())
        for literal, key in MIRRORED.items():
            self.assertEqual(
                monkeyc_byte_literal(literal), bytes.fromhex(fixtures[key]),
                f"{literal} in OrmProtocolTest.mc no longer matches "
                f"golden-packets.json[{key}]; update the Monkey C literal")

    def test_literals_are_whole_packets(self) -> None:
        for literal in MIRRORED:
            self.assertEqual(len(monkeyc_byte_literal(literal)), 20, literal)


if __name__ == "__main__":
    unittest.main()
