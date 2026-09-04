import os
import tempfile
import unittest
from unittest.mock import patch

from openridemirror_tools.paths import repo_root


class RepositoryPathTests(unittest.TestCase):
    def test_explicit_root_override(self):
        expected = repo_root()
        with patch.dict(os.environ, {"OPENRIDEMIRROR_ROOT": str(expected)}, clear=True):
            self.assertEqual(repo_root(), expected)

    def test_finds_root_by_walking_up_from_package(self):
        expected = repo_root()
        with patch.dict(os.environ, {}, clear=True):
            self.assertEqual(repo_root(), expected)

    def test_rejects_invalid_root_override(self):
        with tempfile.TemporaryDirectory() as directory:
            with patch.dict(os.environ, {"OPENRIDEMIRROR_ROOT": directory}, clear=True):
                with self.assertRaisesRegex(RuntimeError, "OPENRIDEMIRROR_ROOT"):
                    repo_root()


if __name__ == "__main__":
    unittest.main()
