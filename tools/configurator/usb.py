"""USB-подключение: поиск платы в COM-портах, чтение API-ключа и прошивка.

pyserial и esptool импортируются лениво: без них остальные модули конфигуратора
работают, а USB-функции сообщают, что нужно установить.
"""

from __future__ import annotations

import contextlib
import json
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

from .core import DeviceError

INFO_REQUEST = b"AUTOMATIKA?\n"
ESPRESSIF_VID = 0x303A
PROBE_BAUD = 115200
CHIP = "esp32s3"
FLASH_BAUD = 921600
FLASH_MODE = "qio"
FLASH_FREQ = "80m"
FLASH_SIZE = "16MB"

# Смещения совпадают с partitions.csv: загрузчик, таблица разделов, otadata
# (сбрасывается на factory), образ приложения, веб-интерфейс (assets).
FLASH_PARTS: tuple[tuple[str, int, int], ...] = (
    ("bootloader.bin", 0x0, 0x8000),
    ("partition-table.bin", 0x8000, 0x1000),
    ("ota_data_initial.bin", 0xF000, 0x2000),
    ("automatika_v2.bin", 0x20000, 0x300000),
    ("assets.bin", 0x920000, 0x100000),
)

# Подписи портов, у которых обычно есть мост для прошивки.
FLASH_PORT_HINTS = ("CH34", "CP210", "USB-SERIAL", "USB SERIAL", "JTAG", "ESPRESSIF")


@dataclass
class PortInfo:
    device: str
    description: str
    vid: int | None
    pid: int | None


def _serial_module():
    try:
        import serial  # type: ignore[import-not-found]
    except ImportError as exc:
        raise DeviceError(
            "не установлен pyserial: pip install -r tools/configurator/requirements.txt"
        ) from exc
    return serial


def list_ports() -> list[PortInfo]:
    serial = _serial_module()
    from serial.tools import list_ports as list_serial_ports  # type: ignore[import-not-found]

    ports = [PortInfo(p.device, p.description or "", p.vid, p.pid)
             for p in list_serial_ports.comports()]
    # Сначала порты Espressif (нативный USB), затем остальные по имени.
    ports.sort(key=lambda p: (p.vid != ESPRESSIF_VID, p.device))
    return ports


def parse_info(raw: bytes | str) -> dict | None:
    """Разбирает строку ответа «AUTOMATIKA?»; None, если это не Automatika."""
    text = raw.decode("utf-8", "replace") if isinstance(raw, bytes) else raw
    text = text.strip()
    if not text.startswith("{"):
        return None
    try:
        data = json.loads(text)
    except ValueError:
        return None
    if not isinstance(data, dict) or data.get("name") != "Automatika":
        return None
    return data


def probe_port(device: str, timeout: float = 2.0) -> dict | None:
    """Спрашивает порт о Automatika. Не сбрасывает плату линиями DTR/RTS."""
    serial = _serial_module()
    try:
        port = serial.Serial(device, PROBE_BAUD, timeout=0.3,
                             dsrdtr=False, rtscts=False)
    except (serial.SerialException, OSError):
        return None
    try:
        port.dtr = False
        port.rts = False
        port.reset_input_buffer()
        port.write(INFO_REQUEST)
        port.flush()
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            info = parse_info(port.readline())
            if info is not None:
                return info
        return None
    except (serial.SerialException, OSError):
        return None
    finally:
        port.close()


def find_device(ports: list[PortInfo] | None = None) -> tuple[PortInfo, dict] | None:
    """Первый порт, который ответил как Automatika, и его данные."""
    for port in ports if ports is not None else list_ports():
        info = probe_port(port.device)
        if info is not None:
            return port, info
    return None


def guess_flash_port(ports: list[PortInfo], cdc_device: str = "") -> str:
    """Порт для прошивки: мост CH34x/CP210x или USB-JTAG, но не CDC платы."""
    candidates = [p for p in ports if p.device != cdc_device]
    for port in candidates:
        text = f"{port.description} {port.device}".upper()
        if any(hint in text for hint in FLASH_PORT_HINTS):
            return port.device
    return candidates[0].device if candidates else ""


def check_idle(info: dict | None) -> None:
    """Не даёт прошивать или обновлять во время процесса."""
    if info is not None and info.get("running"):
        raise DeviceError("процесс запущен: остановите его перед прошивкой")


def locate_artifacts(root: Path) -> dict[str, Path]:
    """Ищет файлы прошивки в папке, в том числе в build/ после idf.py build."""
    if not root.is_dir():
        raise DeviceError(f"папка не найдена: {root}")
    found: dict[str, Path] = {}
    for name, _offset, _limit in FLASH_PARTS:
        matches = sorted(root.rglob(name))
        if matches:
            found[name] = matches[0]
    missing = [name for name, _o, _l in FLASH_PARTS if name not in found]
    if missing:
        raise DeviceError("в папке нет файлов: " + ", ".join(missing))
    return found


def check_sizes(files: dict[str, Path]) -> None:
    for name, _offset, limit in FLASH_PARTS:
        size = files[name].stat().st_size
        if size <= 0:
            raise DeviceError(f"пустой файл: {name}")
        if size > limit:
            raise DeviceError(f"{name}: {size} байт больше раздела ({limit})")


def build_flash_args(port: str, files: dict[str, Path]) -> list[str]:
    args = [
        "--chip", CHIP,
        "--port", port,
        "--baud", str(FLASH_BAUD),
        "--before", "default_reset",
        "--after", "hard_reset",
        "write_flash",
        "--flash_mode", FLASH_MODE,
        "--flash_freq", FLASH_FREQ,
        "--flash_size", FLASH_SIZE,
    ]
    for name, offset, _limit in FLASH_PARTS:
        args.extend([f"0x{offset:X}", str(files[name])])
    return args


class _LineSink:
    """Файловый объект: переводит поток esptool в строки журнала."""

    def __init__(self, log: Callable[[str], None] | None) -> None:
        self._log = log
        self._buf = ""
        self.tail: list[str] = []

    def write(self, text: str) -> int:
        self._buf += text
        while "\n" in self._buf:
            line, self._buf = self._buf.split("\n", 1)
            self._emit(line)
        return len(text)

    def flush(self) -> None:
        if self._buf:
            self._emit(self._buf)
            self._buf = ""

    def _emit(self, line: str) -> None:
        text = line.strip()
        if not text:
            return
        self.tail = (self.tail + [text])[-20:]
        if self._log is not None:
            self._log(text)


def run_esptool(args: list[str], log: Callable[[str], None] | None = None) -> None:
    """Запускает esptool в этом же процессе: работает и из исходников, и в .exe."""
    try:
        import esptool  # type: ignore[import-not-found]
    except ImportError as exc:
        raise DeviceError(
            "не установлен esptool: pip install -r tools/configurator/requirements.txt"
        ) from exc

    sink = _LineSink(log)
    code = 0
    try:
        with contextlib.redirect_stdout(sink), contextlib.redirect_stderr(sink):
            esptool.main(args)
    except SystemExit as exc:
        if isinstance(exc.code, int):
            code = exc.code
        elif exc.code not in (None, 0):
            code = 1
    except Exception as exc:  # noqa: BLE001 — показываем текст ошибки оператору
        sink.write(f"\n{exc}\n")
        code = 1
    finally:
        sink.flush()
    if code != 0:
        raise DeviceError("esptool завершился с ошибкой: " + " | ".join(sink.tail[-5:]))


def flash_usb(port: str, root: Path, *, info: dict | None = None,
              log: Callable[[str], None] | None = None) -> None:
    """Прошивает приложение, веб-интерфейс и загрузчик через esptool."""
    if not port:
        raise DeviceError("не выбран порт для прошивки")
    check_idle(info)
    files = locate_artifacts(root)
    check_sizes(files)
    run_esptool(build_flash_args(port, files), log)
