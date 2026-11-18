#!/usr/bin/env python3
"""Запуск Unity-тестов ESP32-S3 в QEMU без ручного monitor.

Использование:
  python test/qemu/run_qemu_tests.py --build-dir test/build

Возвращает 0, если Unity сообщил 0 failures.
"""
import argparse
import os
import re
import shutil
import socket
import subprocess
import sys
import time


def find_qemu():
    exe = shutil.which("qemu-system-xtensa")
    if exe:
        return exe
    tools = os.environ.get("IDF_TOOLS_PATH", "")
    for root in (tools, os.path.expanduser("~/.espressif/tools")):
        if not root or not os.path.isdir(root):
            continue
        for dirpath, _, files in os.walk(root):
            if "qemu-system-xtensa.exe" in files or "qemu-system-xtensa" in files:
                return os.path.join(dirpath, "qemu-system-xtensa.exe"
                                    if os.name == "nt" else "qemu-system-xtensa")
    return None


def make_images(build_dir):
    subprocess.check_call([
        sys.executable, "-m", "esptool", "--chip=esp32s3", "merge_bin",
        "--output=flash_image.bin", "--fill-flash-size=2MB",
        "--flash_mode", "dio", "--flash_freq", "80m", "--flash_size", "2MB",
        "@flash_args",
    ], cwd=build_dir)

    efuse = os.path.join(build_dir, "qemu_efuse.bin")
    try:
        sys.path.insert(0, os.path.join(os.environ["IDF_PATH"], "tools"))
        from idf_py_actions.qemu_ext import QEMU_TARGETS
        data = QEMU_TARGETS["esp32s3"].default_efuse
        with open(efuse, "wb") as f:
            f.write(data)
    except Exception:
        # efuse не обязателен для базовой загрузки
        if not os.path.exists(efuse):
            with open(efuse, "wb") as f:
                f.write(b"\x00" * 128)
    return os.path.join(build_dir, "flash_image.bin"), efuse


def run(build_dir, port, timeout):
    qemu = find_qemu()
    if not qemu:
        print("qemu-system-xtensa not found; install qemu-xtensa", file=sys.stderr)
        return 2
    flash, efuse = make_images(build_dir)

    args = [
        qemu, "-M", "esp32s3",
        "-drive", f"file={flash},if=mtd,format=raw",
        "-drive", f"file={efuse},if=none,format=raw,id=efuse",
        "-global", "driver=nvram.esp32s3.efuse,property=drive,value=efuse",
        "-global", "driver=timer.esp32s3.timg,property=wdt_disable,value=true",
        "-display", "none",
        "-serial", f"tcp:127.0.0.1:{port},server=on,wait=off",
    ]
    proc = subprocess.Popen(args, stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL)
    try:
        deadline = time.time() + 15
        sock = None
        while time.time() < deadline:
            try:
                sock = socket.create_connection(("127.0.0.1", port), timeout=1)
                break
            except OSError:
                time.sleep(0.2)
        if not sock:
            print("QEMU serial socket did not open", file=sys.stderr)
            return 2

        sock.settimeout(0.5)
        data = bytearray()

        def pump(seconds):
            end = time.time() + seconds
            while time.time() < end:
                try:
                    chunk = sock.recv(4096)
                    if not chunk:
                        break
                    data.extend(chunk)
                except socket.timeout:
                    pass
                except OSError:
                    break

        pump(4)
        sock.sendall(b"\n")
        pump(3)
        sock.sendall(b"*\n")
        pump(timeout)
        text = data.decode("utf-8", errors="replace")
        print(text)
        m = re.search(r"(\d+)\s+Tests\s+(\d+)\s+Failures", text)
        if not m:
            print("Unity summary not found", file=sys.stderr)
            return 1
        tests, failures = int(m.group(1)), int(m.group(2))
        print(f"UNITY: {tests} tests, {failures} failures")
        return 0 if tests > 0 and failures == 0 else 1
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except Exception:
            proc.kill()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", default="test/build")
    ap.add_argument("--port", type=int, default=5555)
    ap.add_argument("--timeout", type=int, default=45)
    args = ap.parse_args()
    return run(os.path.abspath(args.build_dir), args.port, args.timeout)


if __name__ == "__main__":
    raise SystemExit(main())
