"""Статические ассеты APK должны совпадать с генератором SD-карты."""

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import gen_android_assets  # noqa: E402


class AndroidStaticAssets(unittest.TestCase):
    def test_static_assets_are_current(self):
        self.assertEqual(gen_android_assets.main(["--check"]), 0)


if __name__ == "__main__":
    unittest.main()
