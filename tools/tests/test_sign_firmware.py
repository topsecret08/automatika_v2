import os
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from cryptography.hazmat.primitives import serialization  # noqa: E402
from cryptography.hazmat.primitives.asymmetric import ec  # noqa: E402

import sign_firmware as sf  # noqa: E402


def _keys():
    key = ec.generate_private_key(ec.SECP256R1())
    priv = key.private_bytes(serialization.Encoding.PEM,
                             serialization.PrivateFormat.PKCS8,
                             serialization.NoEncryption())
    pub = key.public_key().public_bytes(serialization.Encoding.PEM,
                                        serialization.PublicFormat.SubjectPublicKeyInfo)
    return priv, pub


class SignFirmwareTest(unittest.TestCase):
    def setUp(self):
        self.priv, self.pub = _keys()
        self.image = os.urandom(5000)

    def test_roundtrip_verifies(self):
        signed = sf.sign_image(self.image, self.priv)
        self.assertTrue(sf.verify_image(signed, self.pub))
        image, sig = sf.split_signed(signed)
        self.assertEqual(image, self.image)
        self.assertTrue(8 <= len(sig) <= sf.MAX_SIG)

    def test_trailer_layout_matches_firmware(self):
        signed = sf.sign_image(self.image, self.priv)
        self.assertEqual(signed[-8:], b"ATKSIG01")
        sig_len = struct.unpack(">H", signed[-10:-8])[0]
        self.assertEqual(len(signed), len(self.image) + sig_len + 10)

    def test_tampered_byte_is_rejected(self):
        signed = bytearray(sf.sign_image(self.image, self.priv))
        signed[100] ^= 0x01
        self.assertFalse(sf.verify_image(bytes(signed), self.pub))

    def test_unsigned_image_is_rejected(self):
        self.assertFalse(sf.verify_image(self.image, self.pub))

    def test_wrong_key_is_rejected(self):
        signed = sf.sign_image(self.image, self.priv)
        _, other_pub = _keys()
        self.assertFalse(sf.verify_image(signed, other_pub))

    def test_already_signed_is_refused(self):
        signed = sf.sign_image(self.image, self.priv)
        with self.assertRaises(ValueError):
            sf.sign_image(signed, self.priv)

    def test_pubkey_header_has_uncompressed_point(self):
        header = sf.pubkey_header(self.pub)
        self.assertIn("OTA_SIGN_PUBKEY[65]", header)
        point = sf._load_public(self.pub).public_bytes(
            serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint)
        self.assertEqual(point[0], 0x04)
        self.assertIn("0x04", header)

    def test_cli_sign_and_verify(self):
        with tempfile.TemporaryDirectory() as d:
            src = os.path.join(d, "a.bin")
            key = os.path.join(d, "k.pem")
            pub = os.path.join(d, "p.pem")
            out = os.path.join(d, "a-signed.bin")
            with open(src, "wb") as f:
                f.write(self.image)
            with open(key, "wb") as f:
                f.write(self.priv)
            with open(pub, "wb") as f:
                f.write(self.pub)
            self.assertEqual(sf.main(["sign", src, "--key", key, "--out", out]), 0)
            self.assertEqual(sf.main(["verify", out, "--pub", pub]), 0)
            self.assertEqual(sf.main(["verify", src, "--pub", pub]), 1)


if __name__ == "__main__":
    unittest.main()
