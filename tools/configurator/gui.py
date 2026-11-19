"""Tkinter-конфигуратор: устройство, прошивка по Wi-Fi, параметры, журнал."""

from __future__ import annotations

import os
import sys
import threading
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

from . import release, store, usb
from .core import DeviceClient, DeviceError, UploadCancelled, file_sha256
from .theme import apply_theme

# Исходники: tools/automatika.ico. Собранный EXE: файл добавлен через --add-data в tools/.
ICON_PATH = (Path(sys._MEIPASS) / "tools" if getattr(sys, "frozen", False)
             else Path(__file__).resolve().parent.parent) / "automatika.ico"

CONTROL_FIELDS = [
    ("ctrl_mode", "Режим управления", "0"),
    ("out_dev", "Выход отбора (0 valve / 1 pump)", "0"),
    ("hys", "Гистерезис, °C", "0.5"),
    ("rate_mlmin", "Скорость отбора, мл/мин", "60"),
    ("flow_mlmin", "Полный поток, мл/мин", "60"),
    ("steps_per_ml", "Шагов на мл", "1000"),
    ("regulator", "Регулятор (0..5)", "0"),
    ("reg_gpio", "GPIO регулятора", "-1"),
    ("reg_freq", "Частота PWM, Гц", "5000"),
    ("reg_resolution", "Разрядность PWM", "10"),
    ("reg_inverted", "Инверсия PWM (0/1)", "0"),
]


class App(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        apply_theme(self)
        try:
            self.iconbitmap(default=str(ICON_PATH))
        except tk.TclError:
            pass
        self.title("Automatika — конфигуратор")
        self.geometry("860x620")
        self.minsize(720, 520)

        data = store.load()
        last = data.get("last", {})
        self.host_var = tk.StringVar(value=last.get("host", "automatika.local"))
        self.key_var = tk.StringVar(value=last.get("api_key", ""))
        self.status_var = tk.StringVar(value="Не подключено")
        self.fw_path = tk.StringVar()
        self.fw_info = tk.StringVar(value="Образ не выбран")
        self.require_idle = tk.BooleanVar(value=True)
        self.wait_reboot = tk.BooleanVar(value=True)
        self.verify_tls = tk.BooleanVar(value=True)
        self.progress = tk.DoubleVar(value=0.0)
        self.busy = False

        self.client = DeviceClient(self.host_var.get(), self.key_var.get())
        self._build()

    # ------------------------------------------------------------------
    def _client(self) -> DeviceClient:
        self.client = DeviceClient(
            self.host_var.get().strip(), self.key_var.get().strip(),
            verify_tls=self.verify_tls.get())
        return self.client

    def _build(self) -> None:
        top = ttk.Frame(self, padding=10)
        top.pack(fill="x")

        ttk.Label(top, text="Устройство").grid(row=0, column=0, sticky="w")
        ttk.Entry(top, textvariable=self.host_var, width=28).grid(
            row=0, column=1, sticky="we", padx=4)
        ttk.Label(top, text="API-ключ").grid(row=0, column=2, sticky="w")
        ttk.Entry(top, textvariable=self.key_var, width=24,
                  show="•").grid(row=0, column=3, sticky="we", padx=4)
        ttk.Button(top, text="Найти", command=self.discover).grid(
            row=0, column=4, padx=2)
        ttk.Button(top, text="Подключиться", command=self.refresh_device).grid(
            row=0, column=5, padx=2)
        ttk.Checkbutton(top, text="Проверять TLS", variable=self.verify_tls) \
            .grid(row=0, column=6, padx=4)
        top.columnconfigure(1, weight=1)

        ttk.Label(self, textvariable=self.status_var, padding=(10, 0)) \
            .pack(fill="x")

        self.tabs = ttk.Notebook(self)
        self.tabs.pack(fill="both", expand=True, padx=10, pady=8)
        self.tab_quick = ttk.Frame(self.tabs, padding=10)
        self.tab_device = ttk.Frame(self.tabs, padding=10)
        self.tab_fw = ttk.Frame(self.tabs, padding=10)
        self.tab_params = ttk.Frame(self.tabs, padding=10)
        self.tab_events = ttk.Frame(self.tabs, padding=10)
        self.tabs.add(self.tab_quick, text="Быстрый старт")
        self.tabs.add(self.tab_device, text="Устройство")
        self.tabs.add(self.tab_fw, text="Прошивка")
        self.tabs.add(self.tab_params, text="Параметры")
        self.tabs.add(self.tab_events, text="События")

        self._build_quick()
        self._build_device()
        self._build_firmware()
        self._build_params()
        self._build_events()

    def _build_quick(self) -> None:
        self.release_dir: Path | None = None
        self.port_var = tk.StringVar()

        row = ttk.Frame(self.tab_quick)
        row.pack(fill="x")
        ttk.Label(row, text="Порт прошивки").pack(side="left")
        self.port_box = ttk.Combobox(row, textvariable=self.port_var, width=16,
                                     state="readonly")
        self.port_box.pack(side="left", padx=4)
        ttk.Button(row, text="Обновить список", command=self.refresh_ports).pack(
            side="left", padx=2)
        ttk.Button(row, text="Найти плату", command=self.find_usb).pack(
            side="left", padx=2)

        buttons = ttk.Frame(self.tab_quick)
        buttons.pack(fill="x", pady=10)
        ttk.Button(buttons, text="Обновить до последней версии",
                   command=self.quick_update).pack(side="left")
        ttk.Button(buttons, text="Скачать последний релиз",
                   command=self.download_release).pack(side="left", padx=6)

        self.quick_log = tk.Text(self.tab_quick, height=14, wrap="word")
        self.quick_log.pack(fill="both", expand=True)
        self.refresh_ports(quiet=True)

    def qlog(self, message: str) -> None:
        self.quick_log.insert("end", message + "\n")
        self.quick_log.see("end")

    def _qlog_async(self, message: str) -> None:
        self.after(0, lambda: self.qlog(message))

    def refresh_ports(self, quiet: bool = False) -> None:
        try:
            ports = usb.list_ports()
        except DeviceError as exc:
            if not quiet:
                messagebox.showerror("Automatika", str(exc))
            return
        devices = [p.device for p in ports]
        self.port_box["values"] = devices
        if devices and not self.port_var.get():
            self.port_var.set(usb.guess_flash_port(ports))
        if not quiet:
            self.qlog("Портов найдено: " + (", ".join(devices) or "нет"))

    def find_usb(self) -> None:
        def work():
            ports = usb.list_ports()
            found = usb.find_device(ports)
            return ports, found

        def done(result):
            ports, found = result
            self.port_box["values"] = [p.device for p in ports]
            if found is None:
                self.qlog("Плата Automatika по USB не найдена. Проверьте кабель "
                          "и что прошивка V0.9.6 или новее.")
                return
            port, info = found
            self.port_var.set(usb.guess_flash_port(ports, port.device))
            key = str(info.get("api_key", ""))
            if key:
                self.key_var.set(key)
                store.remember(self.host_var.get().strip() or "automatika.local", key)
            state = "запущен" if info.get("running") else "остановлен"
            self.qlog(f"Плата на {port.device}: версия {info.get('version', '?')}, "
                      f"процесс {state}, API-ключ "
                      f"{'получен' if key else 'не задан'}.")

        self._async(work, done)

    def download_release(self) -> None:
        def work():
            return release.fetch_release_folder()

        def done(result):
            tag, folder = result
            self.release_dir = folder
            self.qlog(f"Релиз {tag} скачан и проверен: {folder}")

        self._async(work, done)

    def quick_update(self) -> None:
        if not messagebox.askyesno(
                "Automatika",
                "Обновить прошивку до последней версии?\n\n"
                "Процесс должен быть остановлен. Настройки и история сохранятся."):
            return

        def work():
            ports = usb.list_ports()
            found = usb.find_device(ports)
            info = found[1] if found else None
            cdc_device = found[0].device if found else ""
            if info is not None:
                usb.check_idle(info)
                key = str(info.get("api_key", ""))
                if key:
                    self.after(0, lambda: self.key_var.set(key))
            else:
                state = self._client().state()
                if state.get("running"):
                    raise DeviceError("процесс запущен: остановите его перед обновлением")

            folder = self.release_dir
            if folder is None or not folder.is_dir():
                self._qlog_async("Скачиваю последний релиз…")
                _tag, folder = release.fetch_release_folder()
                self.release_dir = folder
                self._qlog_async(f"Релиз проверен: {folder}")

            flash_port = self.port_var.get() or usb.guess_flash_port(ports, cdc_device)
            if flash_port and flash_port != cdc_device:
                self._qlog_async(f"Прошивка по USB через {flash_port} (загрузчик, "
                                 "таблица разделов, приложение, веб-интерфейс)…")
                usb.flash_usb(flash_port, folder, info=info,
                              log=lambda line: self._qlog_async(line))
                self._qlog_async("USB-прошивка завершена, плата перезагружается.")
                return "usb"

            self._qlog_async("Порта прошивки нет: обновляю приложение по Wi-Fi. "
                             "Веб-интерфейс этим способом не обновляется.")
            result = self._client().upload_firmware(
                folder / "automatika_v2.bin", require_idle=True)
            self._qlog_async(f"Отправлено {result.size} байт, SHA-256 {result.sha256}")
            return "wifi"

        def done(method):
            self.qlog(f"Обновление завершено ({method}).")
            self.refresh_device()

        self._async(work, done)

    def _build_device(self) -> None:
        self.device_info = tk.Text(self.tab_device, height=14, wrap="word")
        self.device_info.pack(fill="both", expand=True)
        row = ttk.Frame(self.tab_device)
        row.pack(fill="x", pady=6)
        for label, action in (
                ("Старт", lambda: self._action("start")),
                ("Стоп", lambda: self._action("stop")),
                ("Сброс аварии", lambda: self._action("safety")),
                ("Обновить", self.refresh_device)):
            ttk.Button(row, text=label, command=action).pack(
                side="left", padx=2)

    def _build_firmware(self) -> None:
        row = ttk.Frame(self.tab_fw)
        row.pack(fill="x")
        ttk.Entry(row, textvariable=self.fw_path).pack(
            side="left", fill="x", expand=True)
        ttk.Button(row, text="Выбрать…", command=self.pick_firmware).pack(
            side="left", padx=4)
        ttk.Label(self.tab_fw, textvariable=self.fw_info).pack(
            anchor="w", pady=6)

        opts = ttk.Frame(self.tab_fw)
        opts.pack(fill="x")
        ttk.Checkbutton(opts, text="Требовать остановленный процесс",
                        variable=self.require_idle).pack(side="left")
        ttk.Checkbutton(opts, text="Ждать перезагрузку",
                        variable=self.wait_reboot).pack(side="left", padx=12)

        ttk.Progressbar(self.tab_fw, variable=self.progress, maximum=100) \
            .pack(fill="x", pady=8)
        buttons = ttk.Frame(self.tab_fw)
        buttons.pack(fill="x")
        self.upload_btn = ttk.Button(buttons, text="Прошить по Wi-Fi",
                                     command=self.upload)
        self.upload_btn.pack(side="left")
        ttk.Button(buttons, text="Отмена", command=self.cancel_upload).pack(
            side="left", padx=6)

        self.fw_log = tk.Text(self.tab_fw, height=12, wrap="word")
        self.fw_log.pack(fill="both", expand=True, pady=(8, 0))
        self.cancel_event = threading.Event()

    def _build_params(self) -> None:
        self.param_vars: dict[str, tk.StringVar] = {}
        for index, (key, label, default) in enumerate(CONTROL_FIELDS):
            ttk.Label(self.tab_params, text=label).grid(
                row=index, column=0, sticky="w", pady=2)
            var = tk.StringVar(value=default)
            self.param_vars[key] = var
            ttk.Entry(self.tab_params, textvariable=var, width=18).grid(
                row=index, column=1, sticky="w", padx=6)
        row = len(CONTROL_FIELDS)
        ttk.Button(self.tab_params, text="Загрузить",
                   command=self.load_params).grid(row=row, column=0, pady=8)
        ttk.Button(self.tab_params, text="Сохранить",
                   command=self.save_params).grid(row=row, column=1, pady=8)

    def _build_events(self) -> None:
        self.events_text = tk.Text(self.tab_events, wrap="none")
        self.events_text.pack(fill="both", expand=True)
        ttk.Button(self.tab_events, text="Обновить",
                   command=self.load_events).pack(anchor="w", pady=6)

    # ------------------------------------------------------------------
    def log(self, message: str) -> None:
        self.fw_log.insert("end", message + "\n")
        self.fw_log.see("end")

    def _async(self, work, done=None) -> None:
        if self.busy:
            return
        self.busy = True
        self.status_var.set("Выполняется…")

        def runner():
            try:
                result = work()
                error = None
            except Exception as exc:  # noqa: BLE001 — показываем оператору
                result, error = None, exc
            self.after(0, lambda: self._finish(result, error, done))

        threading.Thread(target=runner, daemon=True).start()

    def _finish(self, result, error, done) -> None:
        self.busy = False
        if error is not None:
            self.status_var.set(f"Ошибка: {error}")
            messagebox.showerror("Automatika", str(error))
            return
        self.status_var.set("Готово")
        if done:
            done(result)

    # ------------------------------------------------------------------
    def discover(self) -> None:
        def work():
            from tools.automatika_cli import discover_devices
            return discover_devices()

        def done(devices):
            if not devices:
                self.status_var.set("Устройства не найдены")
                return
            self.host_var.set(devices[0]["host"])
            self.status_var.set(f"Найдено: {len(devices)}")
            store.remember(devices[0]["host"], self.key_var.get())

        self._async(work, done)

    def refresh_device(self) -> None:
        def work():
            client = self._client()
            return client.health(), client.state(), client.safety()

        def done(result):
            health, state, safety = result
            self.device_info.delete("1.0", "end")
            version = health.get("version", "?")
            text = (
                f"Адрес: {self.host_var.get()}\n"
                f"Версия: {version}\n"
                f"Uptime: {health.get('uptime_s', 0)} с\n"
                f"Heap: {health.get('free_heap', 0)} байт\n"
                f"Процесс: {'запущен' if state.get('running') else 'остановлен'}\n"
                f"Мощность: {state.get('power', 0)} %\n"
                f"Куб: {state.get('cube', '—')} °C, "
                f"царга: {state.get('col', '—')} °C\n"
                f"Safety: {safety.get('safety', 0)}, "
                f"E-STOP: {safety.get('estop', 0)}\n"
                f"Реле: enabled={safety.get('relay_enabled', 0)} "
                f"closed={safety.get('relay_closed', 0)} "
                f"forced={safety.get('relay_forced', 0)}\n"
                f"Loop: aggregate={safety.get('loop_aggregate', 0)} "
                f"flags=0x{int(safety.get('loop_flags', 0)):X}\n"
            )
            self.device_info.insert("1.0", text)
            store.remember(self.host_var.get(), self.key_var.get())

        self._async(work, done)

    def _action(self, name: str) -> None:
        def work():
            client = self._client()
            if name == "start":
                return client.start()
            if name == "stop":
                return client.stop()
            return client.reset_safety()

        def done(result):
            self.status_var.set(str(result))
            self.refresh_device()

        self._async(work, done)

    # ------------------------------------------------------------------
    def pick_firmware(self) -> None:
        path = filedialog.askopenfilename(
            title="Образ прошивки", filetypes=[("Firmware", "*.bin"),
                                              ("Все файлы", "*.*")])
        if not path:
            return
        self.fw_path.set(path)
        size = os.path.getsize(path)
        digest = file_sha256(Path(path))
        self.fw_info.set(f"{size} байт · SHA-256 {digest}")

    def upload(self) -> None:
        path = self.fw_path.get()
        if not path:
            messagebox.showwarning("Automatika", "Выберите .bin")
            return
        self.cancel_event.clear()
        self.progress.set(0.0)

        def work():
            client = self._client()
            old_version = str(client.health().get("version", ""))
            self.after(0, lambda: self.log(f"Текущая версия: {old_version}"))

            def progress(sent, total):
                percent = 100.0 * sent / total if total else 0
                self.after(0, lambda: self.progress.set(percent))

            result = client.upload_firmware(
                path, progress, self.cancel_event,
                require_idle=self.require_idle.get())
            self.after(0, lambda: self.log(
                f"Отправлено {result.size} байт, SHA-256 {result.sha256}"))
            if self.wait_reboot.get():
                self.after(0, lambda: self.log("Ожидание перезагрузки…"))
                health = client.wait_for_reboot(old_version)
                self.after(0, lambda: self.log(
                    f"Устройство вернулось: {health.get('version')}"))
            return result

        def done(_result):
            self.progress.set(100.0)
            self.refresh_device()

        self._async(work, done)

    def cancel_upload(self) -> None:
        self.cancel_event.set()
        self.log("Отмена загрузки…")

    # ------------------------------------------------------------------
    def load_params(self) -> None:
        def work():
            return self._client().config()

        def done(cfg):
            mapping = {
                "ctrl_mode": cfg.get("ctrl_mode"),
                "out_dev": cfg.get("out_dev"),
                "hys": cfg.get("hys"),
                "rate_mlmin": cfg.get("rate_mlmin"),
                "flow_mlmin": cfg.get("flow_mlmin"),
                "steps_per_ml": cfg.get("steps_per_ml"),
                "regulator": cfg.get("regulator"),
                "reg_gpio": cfg.get("reg_gpio"),
                "reg_freq": cfg.get("reg_freq"),
                "reg_resolution": cfg.get("reg_resolution"),
                "reg_inverted": cfg.get("reg_inverted"),
            }
            for key, value in mapping.items():
                if value is not None and key in self.param_vars:
                    self.param_vars[key].set(str(value))

        self._async(work, done)

    def save_params(self) -> None:
        body = {}
        for key, var in self.param_vars.items():
            raw = var.get().strip()
            try:
                body[key] = float(raw) if "." in raw else int(raw)
            except ValueError:
                messagebox.showerror("Automatika", f"Некорректное число: {key}")
                return

        def work():
            return self._client().save_control(body)

        def done(result):
            self.status_var.set(f"Сохранено: {result}")

        self._async(work, done)

    def load_events(self) -> None:
        def work():
            return self._client().events(100)

        def done(data):
            self.events_text.delete("1.0", "end")
            events = data.get("events", data if isinstance(data, list) else [])
            for item in events:
                if isinstance(item, dict):
                    self.events_text.insert(
                        "end", f"{item.get('timestamp_ms', '')} "
                               f"{item.get('type', '')} "
                               f"{item.get('msg', '')}\n")
                else:
                    self.events_text.insert("end", f"{item}\n")

        self._async(work, done)


def run(argv: list[str] | None = None) -> int:
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="")
    args = parser.parse_args(argv)
    app = App()
    if args.host:
        app.host_var.set(args.host)
        app.refresh_device()
    app.mainloop()
    return 0
