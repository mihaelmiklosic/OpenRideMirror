import hashlib
import json
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

from openridemirror_tools.map_builder import build, install_pack


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

    def test_installs_allowlisted_web_map_pack(self):
        headers = {
            "OrmMapData.h": "#pragma once\n#include <Arduino.h>\n#define ORM_MAP_ATTRIBUTION \"SAMPLE\"\nORM_MAP_INDEX ORM_MAP_DATA\n",
            "OrmMapLabels.h": "#pragma once\n#include <Arduino.h>\nORM_LABEL_INDEX ORM_LABELS ORM_LABEL_TEXT\n",
            "OrmGreenMask.h": "#pragma once\n#include <Arduino.h>\nORM_GREEN_MIN_LON ORM_GREEN_MASK\n",
        }
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / "OpenRideMirror-map-pack.zip"
            with zipfile.ZipFile(archive, "w") as output:
                for name, contents in headers.items():
                    output.writestr(name, contents)
                output.writestr("map-manifest.json", json.dumps({"schema_version": 1}))
            with patch("openridemirror_tools.map_builder.state_dir", return_value=root / ".orm"):
                installed = install_pack(archive)
            self.assertEqual(set(path.name for path in installed.iterdir()), {*headers, "map-manifest.json"})

    def test_rejects_incomplete_web_map_pack(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            archive = root / "bad.zip"
            with zipfile.ZipFile(archive, "w") as output:
                output.writestr("OrmMapData.h", "not a header")
            with patch("openridemirror_tools.map_builder.state_dir", return_value=root / ".orm"):
                with self.assertRaises(ValueError):
                    install_pack(archive)


if __name__ == "__main__":
    unittest.main()
