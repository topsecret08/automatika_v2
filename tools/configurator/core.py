"""Сетевой клиент конфигуратора: JSON API и потоковая загрузка OTA."""

from __future__ import annotations

import hashlib
import http.client
import json
import ssl
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass
from pathlib import Path

MAX_OTA_BYTES = 3 * 1024 * 1024  # размер слота ota_0/ota_1


class DeviceError(RuntimeError):
    pass


class UploadCancelled(DeviceError):
    pass


@dataclass
class UploadResult:
    path: str
    size: int
    sha256: str
    status: int
    response: dict


def file_sha256(path: Path, chunk: int = 1024 * 1024) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(chunk), b""):
            digest.update(block)
    return digest.hexdigest()


class DeviceClient:
    def __init__(self, host: str, api_key: str = "", timeout: float = 8.0,
                 verify_tls: bool = True, ca_file: str = ""):
        self.host = host.strip()
        self.api_key = api_key
        self.timeout = timeout
        self.verify_tls = verify_tls
        self.ca_file = ca_file

    # ------------------------------------------------------------------
    def _base(self) -> str:
        host = self.host
        if not host.startswith(("http://", "https://")):
            host = "http://" + host
        return host.rstrip("/")

    def _ssl_context(self) -> ssl.SSLContext | None:
        if self._base().startswith("https://"):
            if not self.verify_tls:
                ctx = ssl.create_default_context()
                ctx.check_hostname = False
                ctx.verify_mode = ssl.CERT_NONE
                return ctx
            ctx = ssl.create_default_context()
            if self.ca_file:
                ctx.load_verify_locations(self.ca_file)
            return ctx
        return None

    def request_json(self, path: str, method: str = "GET",
                     body: dict | None = None) -> dict:
        data = None
        headers = {"Accept": "application/json"}
        if body is not None:
            data = json.dumps(body).encode("utf-8")
            headers["Content-Type"] = "application/json"
        if self.api_key:
            headers["X-API-Key"] = self.api_key

        request = urllib.request.Request(self._base() + path, data=data,
                                         method=method, headers=headers)
        try:
            with urllib.request.urlopen(request, timeout=self.timeout,
                                        context=self._ssl_context()) as resp:
                raw = resp.read().decode("utf-8", "replace")
                return json.loads(raw) if raw else {}
        except urllib.error.HTTPError as exc:
            raw = exc.read().decode("utf-8", "replace")
            raise DeviceError(f"HTTP {exc.code}: {raw}") from exc
        except (urllib.error.URLError, TimeoutError, OSError) as exc:
            raise DeviceError(str(exc)) from exc

    # ------------------------------------------------------------------
    def health(self) -> dict:
        return self.request_json("/api/health")

    def state(self) -> dict:
        return self.request_json("/api/state")

    def safety(self) -> dict:
        return self.request_json("/api/safety")

    def config(self) -> dict:
        return self.request_json("/api/config")

    def events(self, limit: int = 50) -> dict:
        return self.request_json(f"/api/events?limit={int(limit)}")

    def start(self, mode: str = "") -> dict:
        return self.request_json("/api/start", "POST",
                                 {"mode": mode} if mode else {})

    def stop(self) -> dict:
        return self.request_json("/api/stop", "POST", {})

    def reset_safety(self) -> dict:
        return self.request_json("/api/safety", "POST", {})

    def power(self, value: float | str) -> dict:
        body = {"mode": "auto"} if value == "auto" else {"power": float(value)}
        return self.request_json("/api/power", "POST", body)

    def save_control(self, fields: dict) -> dict:
        return self.request_json("/api/control", "POST", fields)

    # ------------------------------------------------------------------
    def upload_firmware(self, path: str | Path,
                        progress=None,
                        cancel: threading.Event | None = None,
                        require_idle: bool = True) -> UploadResult:
        firmware = Path(path)
        if not firmware.is_file():
            raise DeviceError(f"файл не найден: {firmware}")

        size = firmware.stat().st_size
        if size <= 0:
            raise DeviceError("пустой образ")
        if size > MAX_OTA_BYTES:
            raise DeviceError(
                f"образ {size} байт больше слота OTA ({MAX_OTA_BYTES})")

        if require_idle:
            state = self.state()
            if state.get("running"):
                raise DeviceError(
                    "процесс запущен: остановите его перед обновлением")

        digest = file_sha256(firmware)
        parsed = urllib.parse.urlsplit(self._base())
        host = parsed.hostname or ""
        port = parsed.port
        if not host:
            raise DeviceError("не задан адрес устройства")

        if parsed.scheme == "https":
            conn = http.client.HTTPSConnection(
                host, port or 443, timeout=max(self.timeout, 30.0),
                context=self._ssl_context())
        else:
            conn = http.client.HTTPConnection(
                host, port or 80, timeout=max(self.timeout, 30.0))

        try:
            conn.putrequest("POST", "/api/ota")
            conn.putheader("Content-Type", "application/octet-stream")
            conn.putheader("Content-Length", str(size))
            conn.putheader("Accept", "application/json")
            if self.api_key:
                conn.putheader("X-API-Key", self.api_key)
            conn.endheaders()

            sent = 0
            with firmware.open("rb") as handle:
                while True:
                    if cancel is not None and cancel.is_set():
                        raise UploadCancelled("загрузка отменена")
                    chunk = handle.read(64 * 1024)
                    if not chunk:
                        break
                    conn.send(chunk)
                    sent += len(chunk)
                    if progress:
                        progress(sent, size)

            response = conn.getresponse()
            raw = response.read().decode("utf-8", "replace")
            try:
                body = json.loads(raw) if raw else {}
            except ValueError:
                body = {"raw": raw}
            if response.status != 200:
                raise DeviceError(f"OTA HTTP {response.status}: {raw}")
            if body.get("status") != "ok":
                raise DeviceError(f"OTA отклонён: {raw}")
            return UploadResult(str(firmware), size, digest,
                                response.status, body)
        except (OSError, http.client.HTTPException) as exc:
            raise DeviceError(f"сеть: {exc}") from exc
        finally:
            conn.close()

    def wait_for_reboot(self, old_version: str, timeout: float = 90.0,
                        interval: float = 1.5) -> dict:
        """Ждёт, пока устройство вернётся с новой версией."""
        deadline = time.monotonic() + timeout
        last_error = ""
        while time.monotonic() < deadline:
            try:
                health = self.health()
                version = str(health.get("version", ""))
                if version and version != old_version:
                    return health
            except DeviceError as exc:
                last_error = str(exc)
            time.sleep(interval)
        raise DeviceError(f"устройство не вернулось за {timeout:.0f} с "
                          f"({last_error})")
