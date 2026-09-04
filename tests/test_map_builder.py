import hashlib
import unittest

from openridemirror_tools.map_builder import build


class MapBuilderTests(unittest.TestCase):
    def config(self):
        return {
            "schema_version": 1,
            "hardware": {"board": "waveshare-esp32-s3-rlcd-4.2"},
            "garmin": {"targets": ["fenix7"]},
            "firmware": {"mode": "live"},
            "map": {"area_type": "sample", "preset": "balanced"},
        }

    def test_sample_is_deterministic_and_nonempty(self):
        output, first = build(self.config(), offline=True)
        digest = hashlib.sha256((output / "OrmMapData.h").read_bytes()).hexdigest()
        _, second = build(self.config(), offline=True)
        self.assertEqual(digest, hashlib.sha256((output / "OrmMapData.h").read_bytes()).hexdigest())
        self.assertEqual(first["preview_sha256"], second["preview_sha256"])
        self.assertGreater(first["road_count"], 0)
        self.assertTrue((output / "map-preview.json").exists())


if __name__ == "__main__":
    unittest.main()
