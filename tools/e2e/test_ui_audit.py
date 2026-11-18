"""Аудит доступности и единообразия PWA."""

from __future__ import annotations


def _audit_ids(page):
    return page.evaluate("""
      () => {
        const seen = new Set();
        const duplicates = [];
        document.querySelectorAll('[id]').forEach((el) => {
          if (seen.has(el.id)) duplicates.push(el.id);
          seen.add(el.id);
        });
        return duplicates;
      }
    """)


def test_no_duplicate_ids(make_page, base_url):
    page = make_page()
    for path in ("/?page=dashboard", "/?page=hardware", "/?tab=prepare"):
        page.goto(base_url + path)
        page.wait_for_timeout(150)
        assert _audit_ids(page) == [], path

    page.goto(base_url + "/?page=dashboard")
    page.wait_for_selector("#chart-card")
    page.locator("#chart-card").click()
    page.wait_for_selector("#temp-modal.open")
    assert _audit_ids(page) == []


def test_controls_have_types_and_labels(make_page, base_url):
    page = make_page()
    for path in ("/?page=dashboard", "/?page=hardware", "/?page=calibration",
                 "/?page=autotune", "/?page=selection"):
        page.goto(base_url + path)
        page.wait_for_timeout(120)
        result = page.evaluate("""
          () => ({
            buttons: Array.from(document.querySelectorAll('button'))
              .filter((b) => !b.getAttribute('type')).length,
            images: Array.from(document.querySelectorAll('img'))
              .filter((img) => !img.hasAttribute('alt')).length,
            emptyLinks: Array.from(document.querySelectorAll('a'))
              .filter((a) => !a.textContent.trim() && !a.getAttribute('aria-label'))
              .length
          })
        """)
        assert result == {"buttons": 0, "images": 0, "emptyLinks": 0}, path


def test_no_horizontal_overflow_on_phone(make_page, base_url):
    page = make_page()
    page.set_viewport_size({"width": 360, "height": 640})
    for path in ("/?page=dashboard", "/?page=hardware", "/?page=calibration"):
        page.goto(base_url + path)
        page.wait_for_timeout(150)
        overflow = page.evaluate(
            "document.documentElement.scrollWidth - document.documentElement.clientWidth"
        )
        assert overflow <= 1, f"{path}: overflow {overflow}px"


def test_menu_groups_have_at_most_four_tasks(make_page, base_url):
    page = make_page()
    for group in ("work", "prepare", "diagnose", "system"):
        page.goto(base_url + f"/?tab={group}")
        page.wait_for_selector(".task-row")
        assert page.locator(".task-row").count() <= 4, group


def test_modal_focus_and_escape(make_page, base_url):
    page = make_page()
    page.goto(base_url + "/?page=dashboard")
    page.wait_for_selector("#chart-card")
    page.locator("#chart-card").click()
    page.wait_for_selector("#temp-modal.open")

    assert page.evaluate("document.activeElement.id") == "temp-close"
    assert page.locator("#chart-card").get_attribute("aria-expanded") == "true"

    page.keyboard.press("Escape")
    page.wait_for_function(
        "!document.querySelector('#temp-modal').classList.contains('open')"
    )
    assert page.locator("#chart-card").get_attribute("aria-expanded") == "false"
