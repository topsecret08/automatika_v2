"""PWA end-to-end fixture: static LittleFS + mocked device API.

Тесты не требуют платы: HTTP-сервер отдаёт реальные файлы `littlefs/`,
а все `/api/*` перехватываются Playwright и отвечают детерминированным JSON.
"""

from __future__ import annotations

import http.server
import json
import threading
from pathlib import Path

import pytest
from playwright.sync_api import sync_playwright

ROOT = Path(__file__).resolve().parents[2]
WEB_DIR = ROOT / "littlefs"

DEFAULT_STATE = {
    "type": 1,
    "stage": 4,
    "cube": 78.5,
    "col": 55.2,
    "power": 42.0,
    "power_auto": 42.0,
    "power_manual": 0,
    "heads": 120,
    "body": 640,
    "tails": 0,
    "pressure": 0.0,
    "vacuum": 0,
    "running": 1,
    "paused": 0,
    "elapsed": 600,
    "safety": 1,
    "estop": 0,
    "estop_healthy": 1,
    "service": 0,
    "level_required_fault": 0,
    "slope0": 0.1,
    "rising0": 1,
    "mode": "distill",
    "ctrl_mode": 0,
    "out_dev": 0,
    "regulator": 0,
    "regulator_name": "none",
    "regulator_ready": 1,
    "regulator_gpio": -1,
    "pwm_freq_hz": 0,
    "pwm_resolution": 0,
    "pwm_inverted": 0,
    "analog_range": 0,
    "analog_min_pct": 0,
    "analog_max_pct": 100,
    "ssvr_power_sqrt": 0,
    "regulator_error": "",
}


class _Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(WEB_DIR), **kwargs)

    def do_GET(self):  # noqa: N802 (stdlib API)
        path = self.path.split("?", 1)[0]
        target = WEB_DIR / path.lstrip("/")
        if path not in ("/", "") and not target.exists():
            self.path = "/index.html"  # SPA fallback
        return super().do_GET()

    def log_message(self, *_args):
        pass


@pytest.fixture(scope="session")
def base_url():
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), _Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        yield f"http://127.0.0.1:{server.server_address[1]}"
    finally:
        server.shutdown()
        thread.join(timeout=5)


@pytest.fixture()
def browser():
    with sync_playwright() as p:
        b = p.chromium.launch()
        try:
            yield b
        finally:
            b.close()


@pytest.fixture()
def context(browser):
    ctx = browser.new_context()
    try:
        yield ctx
    finally:
        ctx.close()


@pytest.fixture()
def api_requests():
    return []


@pytest.fixture()
def make_page(browser, base_url, api_requests):
    """Создаёт страницу с перехваченным API.

    `overrides` позволяет подменить отдельные поля `/api/state`.
    """

    contexts = []

    def factory(state_overrides=None, api_key=None):
        state = dict(DEFAULT_STATE)
        if state_overrides:
            state.update(state_overrides)

        context = browser.new_context(service_workers="block")
        if api_key:
            context.add_init_script(
                "localStorage.setItem('automatika.apiKey', %s);" % json.dumps(api_key)
            )
        page = context.new_page()

        def handle(route, request):
            path = request.url.split("://", 1)[1].split("/", 1)[1]
            path = "/" + path.split("?", 1)[0]
            api_requests.append(
                {
                    "path": path,
                    "method": request.method,
                    "headers": request.headers,
                    "body": request.post_data,
                }
            )

            payload = {}
            if path == "/api/state":
                payload = state
            elif path == "/api/ws-token":
                payload = {"token": "e2e-token"}
            elif path == "/api/health":
                payload = {
                    "level": "ok",
                    "version": "V0.9 beta",
                    "free_heap": 140000,
                    "uptime_s": 600,
                }
            elif path == "/api/param":
                payload = []
            elif path == "/api/profiles":
                payload = []
            elif path == "/api/events":
                payload = []
            elif path == "/api/selection/state":
                payload = {"mode": 0, "algorithm": 0, "stage": 0, "countdown": 0}
            elif path == "/api/i18n":
                payload = {"lang": "ru"}
            elif path in ("/api/start", "/api/stop", "/api/pause", "/api/safety",
                          "/api/power", "/api/control"):
                payload = {"ok": True}

            route.fulfill(
                status=200,
                content_type="application/json",
                body=json.dumps(payload),
            )

        page.route("**/api/**", handle)
        contexts.append(context)
        return page

    try:
        yield factory
    finally:
        for context in contexts:
            context.close()
