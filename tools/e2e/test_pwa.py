"""Сценарные тесты PWA без платы (Playwright + мок API)."""

from __future__ import annotations

import re


def test_dashboard_renders_telemetry(make_page, base_url):
    page = make_page()
    page.goto(base_url + "/?page=dashboard")

    page.wait_for_function(
        "document.querySelector('#temp-cube')?.textContent === '78.5'"
    )
    assert page.locator("#power-value").inner_text() == "42"
    assert page.locator("#tab-bar a").count() == 4
    assert page.locator('[data-action="start"]').is_visible()


def test_alarm_banner_and_status(make_page, base_url):
    page = make_page({"safety": 0, "estop": 1})
    page.goto(base_url + "/?page=dashboard")

    page.wait_for_function(
        "document.querySelector('#connection-label')?.textContent === 'Авария'"
    )
    banner = page.locator("#safety-banner")
    assert banner.is_visible()
    assert "E-STOP" in banner.inner_text()


def test_api_key_is_sent(make_page, base_url, api_requests):
    page = make_page(api_key="E2E-KEY-123")
    page.goto(base_url + "/?page=dashboard")

    page.wait_for_function(
        "document.querySelector('#temp-cube')?.textContent === '78.5'"
    )
    state_calls = [r for r in api_requests if r["path"] == "/api/state"]
    assert state_calls, "PWA не запросила /api/state"
    assert state_calls[0]["headers"].get("x-api-key") == "E2E-KEY-123"


def test_start_posts_to_device(make_page, base_url, api_requests):
    page = make_page({"running": 0}, api_key="E2E-KEY-123")
    page.goto(base_url + "/?page=dashboard")
    page.wait_for_selector('[data-action="start"]')

    page.locator('[data-action="start"]').click()
    page.wait_for_timeout(300)

    posts = [r for r in api_requests
             if r["path"] == "/api/start" and r["method"] == "POST"]
    assert posts, "кнопка «Старт» не отправила POST /api/start"


def test_system_group_navigation(make_page, base_url):
    page = make_page()
    page.goto(base_url + "/?tab=system")

    ota_link = page.locator('[data-page-link="ota"]')
    assert ota_link.is_visible()
    ota_link.click()

    page.wait_for_selector("#ota-url")
    assert page.locator("#page-title").inner_text()


def test_manifest_and_offline_assets(base_url, context):
    manifest = context.request.get(base_url + "/manifest.json")
    assert manifest.ok
    data = manifest.json()
    assert data["name"].startswith("Automatika")
    assert data["display"] in ("standalone", "fullscreen", "minimal-ui")

    offline = context.request.get(base_url + "/offline.html")
    assert offline.ok
    assert re.search(r"offline|офлайн|сеть", offline.text(), re.IGNORECASE)
