import unittest

from openridemirror_tools.cli import parser


class CliTests(unittest.TestCase):
    def test_beginner_shortcuts(self):
        self.assertEqual(parser().parse_args(["demo"]).command, "demo")
        self.assertEqual(parser().parse_args(["live"]).command, "live")
        garmin = parser().parse_args(["garmin"])
        self.assertEqual(garmin.device, "fenix7")

    def test_map_flash_defaults_to_live(self):
        args = parser().parse_args(["map", "flash"])
        self.assertEqual(args.map_command, "flash")
        self.assertEqual(args.mode, "live")
        self.assertIsNone(args.pack)


if __name__ == "__main__":
    unittest.main()
