#!/usr/bin/env python3
"""Automatika CLI и минимальный конфигуратор.

Работает без внешних зависимостей: HTTP через urllib, поиск устройства через
mDNS (A-запрос `automatika.local`) и обычный DNS. GUI — опциональный Tkinter.

Примеры:
    python tools/automatika_cli.py discover
    python tools/automatika_cli.py --host automatika.local health
    python tools/automatika_cli.py --host 192.168.1.50 --key KEY power 40
    python tools/automatika_cli.py gui
"""

from __future__ import annotations

import argparse
import json
import os
import socket
import struct
import sys
import threading
import time
import urllib.error
import urllib.request
from pathlib import Path

CONFIG_PATH = Path.home() / ".automatika.json"
DEFAULT_HOST = "automatika.local"
MDNS_GROUP = "224.0.0.251"
MDNS_PORT = 5353
MDNS_NAME = "automatika.local"


# ---------------------------------------------------------------------------
# Конфигурация
# ---------------------------------------------------------------------------
def load_config() -> dict:
    cfg = {"host": DEFAULT_HOST, "api_key": ""}
    if CONFIG_PATH.exists():
        try:
            data = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
            if isinstance(data, dict):
                cfg.update({k: v for k, v in data.items() if isinstance(v, str)})
        except (OSError, ValueError):
            pass
    if os.environ.get("AUTOMATIKA_HOST"):
        cfg["host"] = os.environ["AUTOMATIKA_HOST"]
    if os.environ.get("AUTOMATIKA_API_KEY"):
        cfg["api_key"] = os.environ["AUTOMATIKA_API_KEY"]
    return cfg


def save_config(host: str, api_key: str) -> None:
    CONFIG_PATH.write_text(
        json.dumps({"host": host, "api_key": api_key}, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )


# ---------------------------------------------------------------------------
# HTTP
# ---------------------------------------------------------------------------
def base_url(host: str) -> str:
    host = (host or DEFAULT_HOST).strip()
    if not host.startswith(("http://", "https://")):
        host = "http://" + host
    return host.rstrip("/")


def api_request(host: str, path: str, method: str = "GET",
                body: dict | None = None, key: str = "",
                timeout: float = 5.0) -> tuple[int, object]:
    url = base_url(host) + path
    data = None
    headers = {"Accept": "application/json"}
    if body is not None:
        data = json.dumps(body).encode("utf-8")
        headers["Content-Type"] = "application/json"
    if key:
        headers["X-API-Key"] = key

    req = urllib.request.Request(url, data=data, method=method, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            raw = resp.read().decode("utf-8", "replace")
            try:
                return resp.status, json.loads(raw)
            except ValueError:
                return resp.status, raw
    except urllib.error.HTTPError as exc:
        raw = exc.read().decode("utf-8", "replace")
        try:
            return exc.code, json.loads(raw)
        except ValueError:
            return exc.code, raw
    except (urllib.error.URLError, TimeoutError, OSError) as exc:
        return 0, {"error": str(exc)}


# ---------------------------------------------------------------------------
# mDNS: A-запрос automatika.local
# ---------------------------------------------------------------------------
def build_mdns_query(name: str = MDNS_NAME) -> bytes:
    labels = name.rstrip(".").split(".")
    qname = b"".join(bytes([len(part)]) + part.encode("idna") for part in labels) + b"\x00"
    # ID=0, flags=0, QDCOUNT=1, остальные счётчики 0
    header = struct.pack("!HHHHHH", 0, 0, 1, 0, 0, 0)
    return header + qname + struct.pack("!HH", 1, 1)  # QTYPE=A, QCLASS=IN


def _skip_name(packet: bytes, offset: int) -> int:
    while offset < len(packet):
        length = packet[offset]
        if length == 0:
            return offset + 1
        if length & 0xC0 == 0xC0:
            return offset + 2
        offset += length + 1
    raise ValueError("truncated DNS name")


def parse_mdns_a(packet: bytes) -> list[str]:
    """Извлекает IPv4-адреса из ответа mDNS."""
    if len(packet) < 12:
        return []
    _id, _flags, qd, an, _ns, _ar = struct.unpack("!HHHHHH", packet[:12])
    offset = 12
    for _ in range(qd):
        offset = _skip_name(packet, offset) + 4
    ips: list[str] = []
    for _ in range(an):
        offset = _skip_name(packet, offset)
        if offset + 10 > len(packet):
            break
        rtype, _rclass, _ttl, rdlen = struct.unpack("!HHIH", packet[offset:offset + 10])
        offset += 10
        if offset + rdlen > len(packet):
            break
        if rtype == 1 and rdlen == 4:
            ips.append(socket.inet_ntoa(packet[offset:offset + 4]))
        offset += rdlen
    return ips


def mdns_discover(timeout: float = 2.0) -> list[str]:
    """Ищет automatika.local: сначала системный DNS, затем multicast mDNS."""
    found: list[str] = []

    try:
        for info in socket.getaddrinfo(MDNS_NAME, None, socket.AF_INET):
            ip = info[4][0]
            if ip not in found:
                found.append(ip)
    except OSError:
        pass

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    try:
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 255)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.settimeout(0.3)
        sock.sendto(build_mdns_query(), (MDNS_GROUP, MDNS_PORT))

        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                packet, _addr = sock.recvfrom(9000)
            except socket.timeout:
                continue
            for ip in parse_mdns_a(packet):
                if ip not in found:
                    found.append(ip)
    except OSError:
        pass
    finally:
        sock.close()

    return found


def discover_devices(timeout: float = 2.0) -> list[dict]:
    devices = []
    for ip in mdns_discover(timeout):
        status, body = api_request(ip, "/api/health", timeout=2.0)
        if status == 200 and isinstance(body, dict):
            devices.append({"host": ip, "health": body})
    if not devices:
        # Последний шанс: имя могло разрешиться, но health не ответил.
        status, body = api_request(DEFAULT_HOST, "/api/health", timeout=2.0)
        if status == 200:
            devices.append({"host": DEFAULT_HOST, "health": body})
    return devices


# ---------------------------------------------------------------------------
# Команды
# ---------------------------------------------------------------------------
def print_json(value) -> None:
    print(json.dumps(value, ensure_ascii=False, indent=2))


def cmd_discover(args, cfg) -> int:
    devices = discover_devices(args.timeout)
    print_json(devices)
    return 0 if devices else 1


def cmd_health(args, cfg) -> int:
    status, body = api_request(cfg["host"], "/api/health")
    print_json(body)
    return 0 if status == 200 else 1


def cmd_state(args, cfg) -> int:
    status, body = api_request(cfg["host"], "/api/state", key=cfg["api_key"])
    print_json(body)
    return 0 if status == 200 else 1


def cmd_safety(args, cfg) -> int:
    if args.reset:
        status, body = api_request(cfg["host"], "/api/safety", "POST", {},
                                   key=cfg["api_key"])
    else:
        status, body = api_request(cfg["host"], "/api/safety", key=cfg["api_key"])
    print_json(body)
    return 0 if status == 200 else 1


def cmd_power(args, cfg) -> int:
    if args.value == "auto":
        body = {"mode": "auto"}
    else:
        try:
            pct = float(args.value)
        except ValueError:
            print("power: ожидается 0..100 или auto", file=sys.stderr)
            return 2
        body = {"power": max(0.0, min(100.0, pct))}
    status, resp = api_request(cfg["host"], "/api/power", "POST", body,
                               key=cfg["api_key"])
    print_json(resp)
    return 0 if status == 200 else 1


def cmd_process(args, cfg) -> int:
    if args.action == "start":
        body = {"mode": args.mode} if args.mode else {}
        path = "/api/start"
    else:
        path = "/api/stop"
        body = {}
    status, resp = api_request(cfg["host"], path, "POST", body, key=cfg["api_key"])
    print_json(resp)
    return 0 if status == 200 else 1


def cmd_config(args, cfg) -> int:
    if args.set_host or args.set_key:
        save_config(args.set_host or cfg["host"], args.set_key or cfg["api_key"])
    shown = load_config()
    if shown.get("api_key"):
        shown["api_key"] = "***"
    print_json(shown)
    return 0


# ---------------------------------------------------------------------------
# GUI (Tkinter, опционально)
# ---------------------------------------------------------------------------
def run_gui() -> int:
    try:
        import tkinter as tk
        from tkinter import messagebox, ttk
    except ImportError:
        print("Tkinter не установлен: используйте CLI-команды.", file=sys.stderr)
        return 2

    cfg = load_config()
    root = tk.Tk()
    root.title("Automatika — конфигуратор")
    root.geometry("560x420")

    host_var = tk.StringVar(value=cfg["host"])
    key_var = tk.StringVar(value=cfg["api_key"])
    status_var = tk.StringVar(value="Не подключено")

    frame = ttk.Frame(root, padding=12)
    frame.pack(fill="both", expand=True)

    ttk.Label(frame, text="Устройство").grid(row=0, column=0, sticky="w")
    ttk.Entry(frame, textvariable=host_var, width=32).grid(row=0, column=1, sticky="we")
    ttk.Label(frame, text="API-ключ").grid(row=1, column=0, sticky="w")
    ttk.Entry(frame, textvariable=key_var, width=32, show="•").grid(row=1, column=1, sticky="we")

    log = tk.Text(frame, height=12, wrap="word")
    log.grid(row=4, column=0, columnspan=3, sticky="nsew", pady=(10, 0))
    frame.rowconfigure(4, weight=1)
    frame.columnconfigure(1, weight=1)

    def show(value):
        log.delete("1.0", "end")
        log.insert("1.0", json.dumps(value, ensure_ascii=False, indent=2))
        status_var.set("Готово")

    def call(path, method="GET", body=None):
        def worker():
            status_var.set("Запрос…")
            status, value = api_request(host_var.get(), path, method, body,
                                        key=key_var.get())
            root.after(0, lambda: (show(value), status_var.set(f"HTTP {status}")))
        threading.Thread(target=worker, daemon=True).start()

    def do_discover():
        def worker():
            status_var.set("Поиск…")
            devices = discover_devices()
            root.after(0, lambda: show(devices))
            if devices:
                root.after(0, lambda: host_var.set(devices[0]["host"]))
        threading.Thread(target=worker, daemon=True).start()

    buttons = [
        ("Найти", do_discover),
        ("Health", lambda: call("/api/health")),
        ("Старт", lambda: call("/api/start", "POST", {})),
        ("Стоп", lambda: call("/api/stop", "POST", {})),
        ("Safety", lambda: call("/api/safety")),
    ]
    row = 2
    for index, (label, action) in enumerate(buttons):
        if index and index % 3 == 0:
            row += 1
        ttk.Button(frame, text=label, command=action).grid(
            row=row, column=index % 3, sticky="we", padx=2, pady=2)

    ttk.Label(frame, textvariable=status_var).grid(row=5, column=0, columnspan=3,
                                                   sticky="w", pady=(8, 0))
    root.mainloop()
    return 0


# ---------------------------------------------------------------------------
def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Automatika CLI")
    parser.add_argument("--host", help="IP или automatika.local")
    parser.add_argument("--key", help="X-API-Key")
    sub = parser.add_subparsers(dest="command")

    p = sub.add_parser("discover", help="поиск устройства в сети")
    p.add_argument("--timeout", type=float, default=2.0)
    p.set_defaults(func=cmd_discover)

    sub.add_parser("health", help="GET /api/health").set_defaults(func=cmd_health)
    sub.add_parser("state", help="GET /api/state").set_defaults(func=cmd_state)

    p = sub.add_parser("safety", help="GET /api/safety или сброс аварии")
    p.add_argument("--reset", action="store_true")
    p.set_defaults(func=cmd_safety)

    p = sub.add_parser("power", help="ручная мощность 0..100 или auto")
    p.add_argument("value")
    p.set_defaults(func=cmd_power)

    p = sub.add_parser("start", help="запуск процесса")
    p.add_argument("mode", nargs="?", default="")
    p.set_defaults(func=cmd_process, action="start")

    sub.add_parser("stop", help="остановка процесса").set_defaults(
        func=cmd_process, action="stop", mode="")

    p = sub.add_parser("config", help="показать/сохранить host и ключ")
    p.add_argument("--set-host")
    p.add_argument("--set-key")
    p.set_defaults(func=cmd_config)

    sub.add_parser("gui", help="окно конфигуратора").set_defaults(func=None)
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    if args.command is None:
        parser.print_help()
        return 2
    if args.command == "gui":
        return run_gui()

    cfg = load_config()
    if getattr(args, "host", None):
        cfg["host"] = args.host
    if getattr(args, "key", None):
        cfg["api_key"] = args.key
    return int(args.func(args, cfg))


if __name__ == "__main__":
    raise SystemExit(main())
