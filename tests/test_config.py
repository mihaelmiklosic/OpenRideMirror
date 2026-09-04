import unittest

from openridemirror_tools.config import validate


class ConfigTests(unittest.TestCase):
    def base(self):
        return {
            "schema_version": 1,
            "hardware": {"board": "waveshare-esp32-s3-rlcd-4.2"},
            "garmin": {"targets": ["fenix7"]},
            "firmware": {"mode": "live"},
            "map": {"area_type": "sample", "preset": "balanced"},
        }

    def test_valid_config(self):
        validate(self.base())

    def test_invalid_board(self):
        config = self.base()
        config["hardware"]["board"] = "generic-esp32"
        with self.assertRaises(ValueError):
            validate(config)

    def test_bbox_requires_four_values(self):
        config = self.base()
        config["map"] = {"area_type": "bbox", "bbox": [1, 2, 3]}
        with self.assertRaises(ValueError):
            validate(config)


if __name__ == "__main__":
    unittest.main()
