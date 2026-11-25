// Снимок 10 превью 800x480 через локально установленный Playwright.
// Запуск: NODE_PATH=/tmp/opencode/hmi-preview-tools/node_modules node render_ten_node.js
const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');

const OUT = process.env.OUT_DIR || '/tmp/opencode/hmi-preview-out';
const SRC = process.env.SRC_DIR || '/tmp/opencode/previews_clean';

// [файл, id экрана, имя PNG] — переключаем класс «on» явно, без hash/query.
const targets = [
  ['hmi_preview.html', 'screen1', '01_rectification'],
  ['hmi_preview.html', 'screen2', '02_main_menu'],
  ['hmi_modes.html', 'prep', '03_preparation'],
  ['hmi_modes.html', 'diag', '04_diagnostics'],
  ['hmi_modes.html', 'sys', '05_system'],
  ['hmi_more.html', 'modes', '06_modes'],
  ['hmi_more.html', 'distill', '07_distillation_work'],
  ['hmi_more.html', 'beer', '08_beer_mash'],
  ['hmi_more.html', 'help', '09_help'],
  ['hmi_more.html', 'calc', '10_calculators'],
];

(async () => {
  fs.mkdirSync(OUT, { recursive: true });
  const browser = await chromium.launch({ headless: true });
  const page = await browser.newPage({ viewport: { width: 800, height: 480 } });

  for (const [file, id, name] of targets) {
    await page.goto(`file://${path.join(SRC, file)}`, { waitUntil: 'load' });
    await page.evaluate((wanted) => {
      document.querySelectorAll('.screen').forEach(e => e.classList.remove('on'));
      const el = document.getElementById(wanted);
      if (el) el.classList.add('on');
    }, id);
    await page.waitForTimeout(350);

    const active = await page.evaluate(() => {
      const el = document.querySelector('.screen.on');
      return el ? el.id : 'NONE';
    });
    if (active !== id) throw new Error(`Не переключился экран ${id}, активен ${active}`);

    await page.locator('.screen.on .frame').screenshot({ path: path.join(OUT, `${name}.png`) });
    console.log(`${name}.png  active=${active}`);
  }

  await browser.close();
})().catch(e => { console.error(e); process.exit(1); });
