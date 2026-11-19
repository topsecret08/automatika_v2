from __future__ import annotations

import http.server
import json
import tempfile
import threading
import unittest
from pathlib import Path

from tools.configurator import core, store


class _OtaHandler(http.server.BaseHTTPRequestHandler):
    body = b""
    key = ""
    running = 0
    health_version = "V0.9.6"
    health_calls = 0

    def _json(self, payload, status=200):
        data = json.dumps(payload).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):  # noqa: N802
        if self.path.startswith("/api/state"):
            return self._json({"running": _OtaHandler.running})
        if self.path.startswith("/api/health"):
            _OtaHandler.health_calls += 1
            version = _OtaHandler.health_version
            if _OtaHandler.health_calls > 1:
                version = "V0.8.1 beta"
            return self._json({"version": version})
        return self._json({"error": "not found"}, 404)

    def do_POST(self):  # noqa: N802
        length = int(self.headers.get("Content-Length", "0"))
        _OtaHandler.body = self.rfile.read(length)
        _OtaHandler.key = self.headers.get("X-API-Key", "")
        return self._json({"status": "ok", "reboot": True})

    def log_message(self, *_args):
        pass


class TestConfigurator(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0),
                                                     _OtaHandler)
        cls.port = cls.server.server_address[1]
        cls.thread = threading.Thread(target=cls.server.serve_forever,
                                      daemon=True)
        cls.thread.start()
        cls.host = f"127.0.0.1:{cls.port}"

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.thread.join(timeout=5)

    def setUp(self):
        _OtaHandler.body = b""
        _OtaHandler.key = ""
        _OtaHandler.running = 0
        _OtaHandler.health_calls = 0
        _OtaHandler.health_version = "V0.9.6"

    def test_upload_streams_and_reports_progress(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "fw.bin"
            path.write_bytes(b"A" * (256 * 1024))
            progress = []
            client = core.DeviceClient(self.host, api_key="KEY")

            result = client.upload_firmware(
                path, progress=lambda sent, total: progress.append((sent, total)))

            self.assertEqual(result.size, path.stat().st_size)
            self.assertEqual(_OtaHandler.key, "KEY")
            self.assertEqual(_OtaHandler.body, path.read_bytes())
            self.assertEqual(result.status, 200)
            self.assertEqual(progress[-1][0], progress[-1][1])
            self.assertEqual(progress[-1][1], path.stat().st_size)

    def test_upload_refuses_running_process(self):
        _OtaHandler.running = 1
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "fw.bin"
            path.write_bytes(b"x")
            client = core.DeviceClient(self.host)
            with self.assertRaises(core.DeviceError):
                client.upload_firmware(path)

    def test_upload_cancel(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "fw.bin"
            path.write_bytes(b"B" * (512 * 1024))
            cancel = threading.Event()

            def progress(_sent, _total):
                cancel.set()

            client = core.DeviceClient(self.host)
            with self.assertRaises(core.UploadCancelled):
                client.upload_firmware(path, progress=progress, cancel=cancel)

    def test_upload_size_limit(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "fw.bin"
            with path.open("wb") as handle:
                handle.seek(core.MAX_OTA_BYTES)
                handle.write(b"x")
            client = core.DeviceClient(self.host)
            with self.assertRaises(core.DeviceError):
                client.upload_firmware(path, require_idle=False)

    def test_wait_for_reboot(self):
        client = core.DeviceClient(self.host)
        health = client.wait_for_reboot("V0.9.6", timeout=3,
                                        interval=0.01)
        self.assertNotEqual(health["version"], "V0.9.6")

    def test_store_roundtrip(self):
        with tempfile.TemporaryDirectory() as tmp:
            old_dir, old_path = store.CONFIG_DIR, store.CONFIG_PATH
            store.CONFIG_DIR = Path(tmp)
            store.CONFIG_PATH = Path(tmp) / "configurator.json"
            try:
                store.remember("192.168.1.10", "SECRET")
                data = store.load()
                self.assertEqual(data["last"]["host"], "192.168.1.10")
                self.assertEqual(data["devices"][0]["api_key"], "SECRET")
            finally:
                store.CONFIG_DIR, store.CONFIG_PATH = old_dir, old_path


if __name__ == "__main__":
    unittest.main()
