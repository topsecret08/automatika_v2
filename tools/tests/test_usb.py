from __future__ import annotations

import hashlib
import sys
import tempfile
import types
import unittest
import zipfile
from pathlib import Path
from unittest import mock

from tools.configurator import release, usb
from tools.configurator.core import DeviceError


def _write(path: Path, size: int = 16) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b"x" * size)
    return path


def _artifacts(root: Path) -> dict[str, Path]:
    return {name: _write(root / name) for name, _o, _l in usb.FLASH_PARTS}


class ParseInfoTest(unittest.TestCase):
    def test_accepts_automatika_reply(self):
        line = b'{"name":"Automatika","version":"V0.9 beta","running":0,"api_key":"ab"}\n'
        info = usb.parse_info(line)
        self.assertEqual(info["version"], "V0.9 beta")
        self.assertEqual(info["api_key"], "ab")

    def test_rejects_other_devices(self):
        self.assertIsNone(usb.parse_info(b"hello\n"))
        self.assertIsNone(usb.parse_info(b'{"name":"Other"}\n'))
        self.assertIsNone(usb.parse_info(b"{broken\n"))
        self.assertIsNone(usb.parse_info(b""))


class CheckIdleTest(unittest.TestCase):
    def test_refuses_running_process(self):
        with self.assertRaises(DeviceError):
            usb.check_idle({"running": 1})

    def test_allows_idle_and_unknown(self):
        usb.check_idle({"running": 0})
        usb.check_idle(None)


class FlashPlanTest(unittest.TestCase):
    def test_offsets_match_partition_table(self):
        offsets = {name: offset for name, offset, _l in usb.FLASH_PARTS}
        self.assertEqual(offsets["bootloader.bin"], 0x0)
        self.assertEqual(offsets["partition-table.bin"], 0x8000)
        self.assertEqual(offsets["ota_data_initial.bin"], 0xF000)
        self.assertEqual(offsets["automatika_v2.bin"], 0x20000)
        self.assertEqual(offsets["assets.bin"], 0x920000)

    def test_flash_args_never_touch_nvs_or_storage(self):
        with tempfile.TemporaryDirectory() as tmp:
            files = _artifacts(Path(tmp))
            args = usb.build_flash_args("COM7", files)
        written = [args[i] for i, a in enumerate(args) if a.startswith("0x")]
        self.assertNotIn("0x9000", written)
        self.assertNotIn("0xA20000", written)
        self.assertEqual(args[args.index("--port") + 1], "COM7")

    def test_locate_finds_files_in_build_layout(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            _write(root / "automatika_v2.bin")
            _write(root / "assets.bin")
            _write(root / "ota_data_initial.bin")
            _write(root / "bootloader" / "bootloader.bin")
            _write(root / "partition_table" / "partition-table.bin")
            files = usb.locate_artifacts(root)
        self.assertEqual(len(files), len(usb.FLASH_PARTS))

    def test_locate_reports_missing(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(DeviceError) as ctx:
                usb.locate_artifacts(Path(tmp))
        self.assertIn("bootloader.bin", str(ctx.exception))

    def test_size_limit_is_enforced(self):
        with tempfile.TemporaryDirectory() as tmp:
            files = _artifacts(Path(tmp))
            files["automatika_v2.bin"] = _write(Path(tmp) / "big.bin",
                                                usb.FLASH_PARTS[3][2] + 1)
            with self.assertRaises(DeviceError):
                usb.check_sizes(files)


class RunEsptoolTest(unittest.TestCase):
    def _fake(self, behaviour):
        module = types.ModuleType("esptool")
        module.main = behaviour
        return module

    def test_streams_lines_and_reports_failure(self):
        def failing(argv):
            print("Connecting...")
            print("Writing at 0x00020000")
            raise SystemExit(2)

        lines: list[str] = []
        with mock.patch.dict(sys.modules, {"esptool": self._fake(failing)}):
            with self.assertRaises(DeviceError) as ctx:
                usb.run_esptool(["write_flash"], lines.append)
        self.assertEqual(lines, ["Connecting...", "Writing at 0x00020000"])
        self.assertIn("Writing at 0x00020000", str(ctx.exception))

    def test_success_returns_without_error(self):
        def ok(argv):
            print("Hard resetting via RTS pin...")

        lines: list[str] = []
        with mock.patch.dict(sys.modules, {"esptool": self._fake(ok)}):
            usb.run_esptool(["write_flash"], lines.append)
        self.assertEqual(lines, ["Hard resetting via RTS pin..."])

    def test_exception_is_reported(self):
        def broken(argv):
            raise RuntimeError("no serial port")

        with mock.patch.dict(sys.modules, {"esptool": self._fake(broken)}):
            with self.assertRaises(DeviceError) as ctx:
                usb.run_esptool(["write_flash"])
        self.assertIn("no serial port", str(ctx.exception))


class PortChoiceTest(unittest.TestCase):
    def test_flash_port_skips_cdc_device(self):
        ports = [
            usb.PortInfo("COM3", "USB Serial Device (COM3)", 0x303A, 0x1001),
            usb.PortInfo("COM5", "USB-SERIAL CH340", 0x1A86, 0x7523),
        ]
        self.assertEqual(usb.guess_flash_port(ports, cdc_device="COM3"), "COM5")

    def test_flash_port_empty_when_nothing(self):
        self.assertEqual(usb.guess_flash_port([]), "")


class ReleaseTest(unittest.TestCase):
    def test_picks_firmware_archive(self):
        release_json = {
            "tag_name": "v0.9.1",
            "assets": [
                {"name": "notes.txt", "browser_download_url": "https://x/n"},
                {"name": "automatika-v0.9.1.zip",
                 "browser_download_url": "https://github.com/x/a.zip"},
            ],
        }
        tag, name, url = release.pick_release_asset(release_json)
        self.assertEqual((tag, name), ("v0.9.1", "automatika-v0.9.1.zip"))
        self.assertTrue(url.startswith("https://"))

    def test_rejects_plain_http_link(self):
        with self.assertRaises(DeviceError):
            release.pick_release_asset({"assets": [
                {"name": "automatika-x.zip", "browser_download_url": "http://x/a.zip"}]})

    def test_rejects_release_without_archive(self):
        with self.assertRaises(DeviceError):
            release.pick_release_asset({"assets": []})

    def test_extract_blocks_path_traversal(self):
        with tempfile.TemporaryDirectory() as tmp:
            archive = Path(tmp) / "bad.zip"
            with zipfile.ZipFile(archive, "w") as zf:
                zf.writestr("../evil.bin", b"x")
            with self.assertRaises(DeviceError):
                release.extract(archive, Path(tmp) / "out")

    def test_verify_sums_detects_tampering(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            data = b"firmware"
            (root / "automatika_v2.bin").write_bytes(data)
            digest = hashlib.sha256(data).hexdigest()
            (root / "SHA256SUMS.txt").write_text(
                f"{digest}  automatika_v2.bin\n", encoding="utf-8")
            self.assertEqual(release.verify_sums(root), 1)
            (root / "automatika_v2.bin").write_bytes(b"tampered")
            with self.assertRaises(DeviceError):
                release.verify_sums(root)


if __name__ == "__main__":
    unittest.main()
