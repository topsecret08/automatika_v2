#!/usr/bin/env python3
"""Подпись образа прошивки для OTA: ECDSA P-256 по SHA-256.

Хвост, который дописывается в конец образа:
    [DER-подпись][длина подписи, 2 байта BE][b"ATKSIG01"]
Подпись считается по всем байтам образа до хвоста. Прошивка (ota_signature.c)
проверяет тот же формат тем же открытым ключом (main/ota/ota_pubkey.h).

Команды:
    sign    IN.bin --key PRIVATE.pem --out OUT.bin
    verify  FILE.bin --pub PUBLIC.pem
    header  --pub PUBLIC.pem --out main/ota/ota_pubkey.h
    keygen  --private PRIVATE.pem --pub PUBLIC.pem
"""
import argparse
import struct
import sys

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec

MAGIC = b"ATKSIG01"
MAX_SIG = 80
TRAILER_FIXED = 2 + len(MAGIC)


def _load_private(pem: bytes) -> ec.EllipticCurvePrivateKey:
    key = serialization.load_pem_private_key(pem, password=None)
    if not isinstance(key, ec.EllipticCurvePrivateKey) or key.curve.name != "secp256r1":
        raise ValueError("ключ должен быть ECDSA P-256")
    return key


def _load_public(pem: bytes) -> ec.EllipticCurvePublicKey:
    key = serialization.load_pem_public_key(pem)
    if not isinstance(key, ec.EllipticCurvePublicKey) or key.curve.name != "secp256r1":
        raise ValueError("ключ должен быть ECDSA P-256")
    return key


def sign_image(image: bytes, private_pem: bytes) -> bytes:
    if image.endswith(MAGIC):
        raise ValueError("образ уже подписан")
    sig = _load_private(private_pem).sign(image, ec.ECDSA(hashes.SHA256()))
    if len(sig) > MAX_SIG:
        raise ValueError("подпись слишком длинная")
    return image + sig + struct.pack(">H", len(sig)) + MAGIC


def split_signed(blob: bytes) -> tuple[bytes, bytes]:
    if len(blob) < TRAILER_FIXED + 8 or not blob.endswith(MAGIC):
        raise ValueError("нет хвоста подписи")
    sig_len = struct.unpack(">H", blob[-TRAILER_FIXED:-len(MAGIC)])[0]
    if not 8 <= sig_len <= MAX_SIG or sig_len + TRAILER_FIXED >= len(blob):
        raise ValueError("некорректная длина подписи")
    image_len = len(blob) - TRAILER_FIXED - sig_len
    return blob[:image_len], blob[image_len:image_len + sig_len]


def verify_image(blob: bytes, public_pem: bytes) -> bool:
    try:
        image, sig = split_signed(blob)
    except ValueError:
        return False
    try:
        _load_public(public_pem).verify(sig, image, ec.ECDSA(hashes.SHA256()))
    except InvalidSignature:
        return False
    return True


def pubkey_header(public_pem: bytes) -> str:
    point = _load_public(public_pem).public_bytes(
        serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint)
    body = ", ".join("0x%02x" % b for b in point)
    return (
        "/* Публичный ключ подписи OTA (ECDSA P-256). Приватный ключ хранится только в CI-секрете.\n"
        "   Генерируется: python tools/sign_firmware.py header --pub ... --out ... */\n"
        "#pragma once\n"
        "#include <stdint.h>\n\n"
        "static const uint8_t OTA_SIGN_PUBKEY[%d] = {%s};\n" % (len(point), body)
    )


def _keygen(private_path: str, public_path: str) -> None:
    key = ec.generate_private_key(ec.SECP256R1())
    with open(private_path, "wb") as f:
        f.write(key.private_bytes(serialization.Encoding.PEM,
                                  serialization.PrivateFormat.PKCS8,
                                  serialization.NoEncryption()))
    with open(public_path, "wb") as f:
        f.write(key.public_key().public_bytes(serialization.Encoding.PEM,
                                              serialization.PublicFormat.SubjectPublicKeyInfo))


def main(argv: list[str]) -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)

    s = sub.add_parser("sign")
    s.add_argument("image")
    s.add_argument("--key", required=True)
    s.add_argument("--out", required=True)

    v = sub.add_parser("verify")
    v.add_argument("image")
    v.add_argument("--pub", required=True)

    h = sub.add_parser("header")
    h.add_argument("--pub", required=True)
    h.add_argument("--out", required=True)

    k = sub.add_parser("keygen")
    k.add_argument("--private", required=True)
    k.add_argument("--pub", required=True)

    args = p.parse_args(argv)
    if args.cmd == "sign":
        with open(args.image, "rb") as f:
            data = f.read()
        with open(args.key, "rb") as f:
            key = f.read()
        with open(args.out, "wb") as f:
            f.write(sign_image(data, key))
        print("signed", args.out)
    elif args.cmd == "verify":
        with open(args.image, "rb") as f:
            data = f.read()
        with open(args.pub, "rb") as f:
            pub = f.read()
        ok = verify_image(data, pub)
        print("OK" if ok else "BAD SIGNATURE")
        return 0 if ok else 1
    elif args.cmd == "header":
        with open(args.pub, "rb") as f:
            pub = f.read()
        with open(args.out, "w", encoding="utf-8", newline="\n") as f:
            f.write(pubkey_header(pub))
        print("wrote", args.out)
    elif args.cmd == "keygen":
        _keygen(args.private, args.pub)
        print("keys written")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
