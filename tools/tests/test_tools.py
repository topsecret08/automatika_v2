from __future__ import annotations

import http.server
import json
import socket
import struct
import sys
import tempfile
import threading
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools import automatika_cli as cli  # noqa: E402
from tools import gen_api_docs, gen_release_metadata  # noqa: E402


class TestCli(unittest.TestCase):
    def test_base_url(self):
        self.assertEqual(cli.base_url("192.168.1.5"), "http://192.168.1.5")
        self.assertEqual(cli.base_url("http://host/"), "http://host")
        self.assertEqual(cli.base_url("https://host"), "https://host")

    def test_mdns_query_contains_name(self):
        packet = cli.build_mdns_query("automatika.local")
        self.assertEqual(packet[:4], b"\x00\x00\x00\x00")
        self.assertIn(b"automatika", packet)
        self.assertTrue(packet.endswith(b"\x00\x00\x01\x00\x01"))

    def test_mdns_parse_a_record(self):
        header = struct.pack("!HHHHHH", 0, 0x8400, 0, 1, 0, 0)
        answer = (b"\xc0\x0c" + struct.pack("!HHIH", 1, 1, 120, 4)
                  + socket.inet_aton("192.168.1.50"))
        self.assertEqual(cli.parse_mdns_a(header + answer), ["192.168.1.50"])
        self.assertEqual(cli.parse_mdns_a(b"short"), [])


class _Handler(http.server.BaseHTTPRequestHandler):
    key = ""

    def do_GET(self):  # noqa: N802
        _Handler.key = self.headers.get("X-API-Key", "")
        body = json.dumps({"ok": True, "path": self.path}).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *_args):
        pass


class TestHttp(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), _Handler)
        cls.port = cls.server.server_address[1]
        cls.thread = threading.Thread(target=cls.server.serve_forever,
                                      daemon=True)
        cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.thread.join(timeout=5)

    def test_get_with_key(self):
        status, body = cli.api_request(f"127.0.0.1:{self.port}", "/api/state",
                                       key="SECRET")
        self.assertEqual(status, 200)
        self.assertEqual(_Handler.key, "SECRET")
        self.assertEqual(body["path"], "/api/state")


class TestDocs(unittest.TestCase):
    def test_generated_api_is_current(self):
        self.assertEqual(gen_api_docs.main(["--check"]), 0)


class TestReleaseMetadata(unittest.TestCase):
    def test_sha256_and_sbom(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "fw.bin"
            path.write_bytes(b"automatika")
            digest = gen_release_metadata.sha256(path)
            self.assertEqual(len(digest), 64)

            sbom = gen_release_metadata.build_sbom(
                "V0.9.6", "abc123", 1700000000,
                [{"name": "lvgl", "version": "8.3.11", "source": "registry",
                  "license": "MIT", "license_file": ""}],
                [{"path": "fw.bin", "size": 10, "sha256": digest}],
            )
            self.assertEqual(sbom["spdxVersion"], "SPDX-2.3")
            self.assertEqual(len(sbom["packages"]), 2)
            self.assertEqual(sbom["packages"][0]["licenseDeclared"], "MIT")
            self.assertEqual(
                sbom["packages"][0]["checksums"][0]["checksumValue"], digest)

    def test_profiles_have_optional_flags(self):
        base = ROOT / "tools" / "config_profiles"
        for name in ("minimal.defaults", "full.defaults"):
            text = (base / name).read_text(encoding="utf-8")
            self.assertIn("CONFIG_LOOP_VERIFY_ENABLE=", text)
            self.assertIn("CONFIG_SAFETY_RELAY_ENABLE=", text)


if __name__ == "__main__":
    unittest.main()
