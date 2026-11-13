// Automatika PWA — iOS/Liquid Glass UI.
//
// Работает только с реальными эндпоинтами прошивки:
//   GET  /api/state, /api/control, /api/health, /api/events
//   POST /api/start, /api/pause, /api/stop, /api/power, /api/safety,
//        /api/control, /api/config/generate-key
// Телеметрия — /ws/telemetry. Никаких несуществующих /api/fraction
// и /api/sensors/* здесь нет.

const $ = (sel, root = document) => root.querySelector(sel);
const $$ = (sel, root = document) => Array.from(root.querySelectorAll(sel));

const STORAGE_KEY = 'automatika.apiKey';
const APP_VERSION = '2.9.3-ios.1';

const STAGE_NAMES = [
  'Ожидание', 'Нагрев', 'Стабилизация', 'Головы', 'Тело',
  'Хвосты', 'Охлаждение', 'Готово', 'Проверка', 'Прогрев',
  'Работа', 'Ошибка'
];

const state = {
  tc: NaN,
  ts: NaN,
  power: 0,
  rate: 0,
  level: 0,
  stage: 0,
  running: 0,
  paused: 0,
  heads: 0,
  body: 0,
  tails: 0,
  safety: 1,
  estop: 0,
  online: false
};

let ws = null;
let wsRetry = null;
let chart = null;
let chartData = [];
let currentPage = 'dashboard';
let powerDragging = false;
let deferredPrompt = null;

// ------------------------------------------------------------------
// API
// ------------------------------------------------------------------
function apiKey() {
  try { return localStorage.getItem(STORAGE_KEY) || ''; } catch (_) { return ''; }
}

function setApiKey(key) {
  try {
    if (key) localStorage.setItem(STORAGE_KEY, key);
    else localStorage.removeItem(STORAGE_KEY);
  } catch (_) { /* приватный режим */ }
}

async function api(path, options = {}, retry = true) {
  const headers = Object.assign(
    { 'Content-Type': 'application/json' },
    options.headers || {}
  );
  const key = apiKey();
  if (key) headers['X-API-Key'] = key;

  const response = await fetch(path, Object.assign({}, options, { headers }));

  if (response.status === 401 && retry) {
    const entered = window.prompt('Введите API-ключ Automatika');
    if (entered) {
      setApiKey(entered.trim());
      return api(path, options, false);
    }
    throw new Error('unauthorized');
  }
  return response;
}

async function postJson(path, body) {
  return api(path, {
    method: 'POST',
    body: body === undefined ? undefined : JSON.stringify(body)
  });
}

// ------------------------------------------------------------------
// Toast и статус
// ------------------------------------------------------------------
let toastTimer = null;
function toast(message) {
  let el = $('#toast');
  if (!el) {
    el = document.createElement('div');
    el.id = 'toast';
    el.className = 'toast';
    el.setAttribute('role', 'status');
    document.body.appendChild(el);
  }
  el.textContent = message;
  el.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => el.classList.remove('show'), 2800);
}

function setStatus(online) {
  state.online = online;
  const dot = $('#connection-status');
  const label = $('#connection-label');
  if (!dot || !label) return;

  const alarm = state.estop || !state.safety;
  dot.className = 'status-dot ' + (alarm ? 'warn' : (online ? 'online' : 'offline'));
  label.textContent = alarm ? 'Авария' : (online ? 'Online' : 'Offline');
}

// ------------------------------------------------------------------
// Телеметрия
// ------------------------------------------------------------------
function wsUrl() {
  const proto = location.protocol === 'https:' ? 'wss://' : 'ws://';
  return proto + location.host + '/ws/telemetry';
}

function connectWS() {
  if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) return;

  try {
    ws = new WebSocket(wsUrl());
  } catch (_) {
    scheduleWS();
    return;
  }

  ws.onopen = () => {
    if (wsRetry) { clearTimeout(wsRetry); wsRetry = null; }
    setStatus(true);
  };

  ws.onmessage = (event) => {
    let frame;
    try { frame = JSON.parse(event.data); } catch (_) { return; }
    applyTelemetry(frame);
  };

  ws.onerror = () => { try { ws.close(); } catch (_) {} };
  ws.onclose = () => {
    setStatus(false);
    scheduleWS();
  };
}

function scheduleWS() {
  if (wsRetry) return;
  wsRetry = setTimeout(() => { wsRetry = null; connectWS(); }, 3000);
}

function applyTelemetry(frame) {
  if (typeof frame.tc === 'number') state.tc = frame.tc;
  if (typeof frame.ts === 'number') state.ts = frame.ts;
  if (typeof frame.p === 'number') state.power = frame.p;
  if (typeof frame.m === 'number') state.rate = frame.m;
  if (typeof frame.lv === 'number') state.level = frame.lv;
  if (typeof frame.stg === 'number') state.stage = frame.stg;
  if (typeof frame.r === 'number') state.running = frame.r;
  if (typeof frame.sa === 'number') state.safety = frame.sa;
  if (typeof frame.es === 'number') state.estop = frame.es;

  renderLiveValues();
  pushChartPoint();
}

async function pollState() {
  if (document.hidden) return;
  try {
    const response = await fetch('/api/state');
    if (!response.ok) return;
    const data = await response.json();
    if (typeof data.tc !== 'undefined') state.tc = data.cube;
    if (typeof data.col !== 'undefined') state.ts = data.col;
    if (typeof data.power !== 'undefined') state.power = data.power;
    if (typeof data.stage !== 'undefined') state.stage = data.stage;
    if (typeof data.running !== 'undefined') state.running = data.running;
    if (typeof data.paused !== 'undefined') state.paused = data.paused;
    if (typeof data.heads !== 'undefined') state.heads = data.heads;
    if (typeof data.body !== 'undefined') state.body = data.body;
    if (typeof data.tails !== 'undefined') state.tails = data.tails;
    if (typeof data.safety !== 'undefined') state.safety = data.safety;
    if (typeof data.estop !== 'undefined') state.estop = data.estop;
    renderLiveValues();
    pushChartPoint();
  } catch (_) {
    setStatus(false);
  }
}

// ------------------------------------------------------------------
// Форматирование
// ------------------------------------------------------------------
function temp(value) {
  return Number.isFinite(value) ? value.toFixed(1) : '--.-';
}

function fractionFromStage(stage) {
  if (stage === 3) return { key: 'heads', label: 'Головы' };
  if (stage === 4) return { key: 'body', label: 'Тело' };
  if (stage === 5) return { key: 'tails', label: 'Хвосты' };
  if (stage === 7) return { key: 'done', label: 'Готово' };
  return { key: 'idle', label: '—' };
}

function volumeMl() {
  return Math.max(0, (state.heads || 0) + (state.body || 0) + (state.tails || 0));
}

// ------------------------------------------------------------------
// Роутер
// ------------------------------------------------------------------
const PAGES = {
  dashboard: { title: 'Дистилляция', render: renderDashboard },
  fractions: { title: 'Фракции', render: renderFractions },
  settings: { title: 'Настройки', render: renderSettings },
  log: { title: 'Журнал', render: renderLog }
};

function pageFromLocation() {
  const params = new URLSearchParams(location.search);
  const query = params.get('page');
  const hash = (location.hash || '').replace('#', '');
  const page = query || hash || 'dashboard';
  return PAGES[page] ? page : 'dashboard';
}

function navigate(page, push = true) {
  if (!PAGES[page]) page = 'dashboard';
  currentPage = page;
  if (push) {
    const url = new URL(location.href);
    url.searchParams.set('page', page);
    url.hash = '';
    history.pushState({ page }, '', url.toString());
  }
  const title = $('#page-title');
  if (title) title.textContent = PAGES[page].title;
  $$('.tab-bar a').forEach((a) => {
    a.classList.toggle('active', a.dataset.page === page);
  });
  chart = null;
  PAGES[page].render();
  renderLiveValues();
}

function router() {
  navigate(pageFromLocation(), false);
}

// ------------------------------------------------------------------
// Dashboard
// ------------------------------------------------------------------
function renderDashboard() {
  const app = $('#app');
  if (!app) return;

  app.innerHTML = `
    <div id="safety-banner" class="banner banner-danger hidden" role="alert"></div>

    <section class="section">
      <div class="card hero-card">
        <div class="hero-label">Пар (скорр.)</div>
        <div class="hero-value"><span id="temp-steam">--.-</span><span class="hero-unit">°C</span></div>
        <div class="hero-meta">
          <span id="fraction-badge" class="badge badge-idle">—</span>
          <span id="stage-label" class="badge badge-idle">Ожидание</span>
        </div>
      </div>
    </section>

    <section class="section">
      <div class="kpi-grid">
        <div class="kpi">
          <div class="kpi-label">Куб</div>
          <div class="kpi-value"><span id="temp-cube">--.-</span><span class="kpi-unit">°C</span></div>
        </div>
        <div class="kpi">
          <div class="kpi-label">Мощность</div>
          <div class="kpi-value"><span id="power-value">0</span><span class="kpi-unit">%</span></div>
        </div>
        <div class="kpi">
          <div class="kpi-label">Отбор</div>
          <div class="kpi-value"><span id="rate-value">--</span><span class="kpi-unit">мл/мин</span></div>
        </div>
        <div class="kpi">
          <div class="kpi-label">Объём</div>
          <div class="kpi-value"><span id="volume-value">0</span><span class="kpi-unit">мл</span></div>
        </div>
      </div>
    </section>

    <section class="section">
      <div class="card">
        <div class="row">
          <span class="card-row-title">Мощность нагрева</span>
          <span id="power-slider-value" class="headline">0%</span>
        </div>
        <div class="slider-row mt-8">
          <input id="power-slider" type="range" min="0" max="100" step="1" value="0"
                 aria-label="Мощность нагрева, проценты">
        </div>
      </div>
    </section>

    <section class="section">
      <div class="card chart-card">
        <div class="chart-head">
          <span class="card-row-title">Динамика</span>
          <span class="chart-legend">
            <span><span class="legend-dot legend-cube"></span>куб</span>
            <span><span class="legend-dot legend-steam"></span>пар</span>
          </span>
        </div>
        <div class="chart-wrap"><canvas id="chart"></canvas></div>
      </div>
    </section>

    <section class="section">
      <div class="btn-row">
        <button class="btn btn-primary" type="button" data-action="start">Старт</button>
        <button class="btn btn-secondary" type="button" data-action="pause">Пауза</button>
        <button class="btn btn-danger" type="button" data-action="stop">Стоп</button>
      </div>
      <div class="btn-row mt-8">
        <button class="btn btn-secondary btn-block" type="button" data-action="safety">
          Сброс аварии
        </button>
      </div>
    </section>
  `;

  const slider = $('#power-slider');
  if (slider) {
    slider.addEventListener('pointerdown', () => { powerDragging = true; });
    slider.addEventListener('pointerup', () => { powerDragging = false; });
    slider.addEventListener('input', () => {
      const label = $('#power-slider-value');
      if (label) label.textContent = slider.value + '%';
    });
    slider.addEventListener('change', async () => {
      powerDragging = false;
      try {
        const response = await postJson('/api/power', { power: Number(slider.value) });
        if (!response.ok) throw new Error(String(response.status));
        toast('Мощность: ' + slider.value + '%');
      } catch (_) {
        toast('Не удалось изменить мощность');
      }
    });
  }

  initChart();
  renderLiveValues();
}

function renderLiveValues() {
  if (currentPage !== 'dashboard') {
    updateStatusOnly();
    return;
  }

  const steam = $('#temp-steam');
  if (!steam) return;

  steam.textContent = temp(state.ts);
  const cube = $('#temp-cube');
  if (cube) cube.textContent = temp(state.tc);

  const power = $('#power-value');
  if (power) power.textContent = String(Math.round(state.power || 0));

  const rate = $('#rate-value');
  if (rate) rate.textContent = state.rate > 0 ? state.rate.toFixed(1) : '--';

  const volume = $('#volume-value');
  if (volume) volume.textContent = String(Math.round(volumeMl()));

  const stageLabel = $('#stage-label');
  if (stageLabel) stageLabel.textContent = STAGE_NAMES[state.stage] || '—';

  const fraction = fractionFromStage(state.stage);
  const badge = $('#fraction-badge');
  if (badge) {
    badge.textContent = fraction.label;
    badge.className = 'badge badge-' + fraction.key;
  }

  const slider = $('#power-slider');
  const sliderLabel = $('#power-slider-value');
  if (slider && !powerDragging) {
    slider.value = String(Math.round(state.power || 0));
    if (sliderLabel) sliderLabel.textContent = slider.value + '%';
  }

  const banner = $('#safety-banner');
  if (banner) {
    if (state.estop) {
      banner.textContent = 'E-STOP активен. Нагрев снят, проверьте причину.';
      banner.classList.remove('hidden');
    } else if (!state.safety) {
      banner.textContent = 'Сработала защита. Проверьте журнал и устраните причину.';
      banner.classList.remove('hidden');
    } else {
      banner.classList.add('hidden');
    }
  }

  updateStatusOnly();
}

function updateStatusOnly() {
  setStatus(state.online);
}

// ------------------------------------------------------------------
// График
// ------------------------------------------------------------------
function initChart() {
  const canvas = $('#chart');
  if (!canvas) {
    chart = null;
    return;
  }

  const rect = canvas.parentElement.getBoundingClientRect();
  const dpr = Math.max(1, window.devicePixelRatio || 1);
  canvas.width = Math.max(1, Math.floor(rect.width * dpr));
  canvas.height = Math.max(1, Math.floor(rect.height * dpr));

  const ctx = canvas.getContext('2d');
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

  chart = { canvas, ctx, width: rect.width, height: rect.height };
  drawChart();
}

function pushChartPoint() {
  if (!Number.isFinite(state.tc) || !Number.isFinite(state.ts)) return;
  chartData.push({ tc: state.tc, ts: state.ts });
  if (chartData.length > 300) chartData.shift();
  drawChart();
}

function cssVar(name) {
  return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
}

function drawChart() {
  if (!chart || !chart.ctx) return;
  const { ctx, width, height } = chart;
  if (!width || !height) return;

  ctx.clearRect(0, 0, width, height);

  const values = chartData.flatMap((p) => [p.tc, p.ts]).filter(Number.isFinite);
  if (values.length < 2) return;

  let min = Math.min(...values) - 2;
  let max = Math.max(...values) + 2;
  if (max - min < 1) { max += 0.5; min -= 0.5; }

  ctx.lineWidth = 0.5;
  ctx.strokeStyle = cssVar('--separator') || 'rgba(120,120,128,0.2)';
  for (let i = 0; i <= 3; i++) {
    const y = (i / 3) * height;
    ctx.beginPath();
    ctx.moveTo(0, y);
    ctx.lineTo(width, y);
    ctx.stroke();
  }

  const x = (index) => (index / Math.max(1, chartData.length - 1)) * width;
  const y = (value) => height - ((value - min) / (max - min)) * height;

  const line = (key, color) => {
    ctx.beginPath();
    ctx.lineWidth = 2;
    ctx.lineJoin = 'round';
    ctx.lineCap = 'round';
    ctx.strokeStyle = color;
    chartData.forEach((point, index) => {
      if (!Number.isFinite(point[key])) return;
      const px = x(index);
      const py = y(point[key]);
      if (index === 0) ctx.moveTo(px, py);
      else ctx.lineTo(px, py);
    });
    ctx.stroke();
  };

  line('tc', cssVar('--accent') || '#007aff');
  line('ts', cssVar('--orange') || '#ff9500');
}

// ------------------------------------------------------------------
// Действия
// ------------------------------------------------------------------
async function runAction(action) {
  const endpoints = {
    start: '/api/start',
    pause: '/api/pause',
    stop: '/api/stop',
    safety: '/api/safety'
  };
  const path = endpoints[action];
  if (!path) return;

  if (!apiKey()) {
    toast('Сначала задайте API-ключ в Настройках');
    return;
  }

  try {
    const response = await postJson(path, {});
    if (!response.ok) {
      let message = 'Ошибка ' + response.status;
      try {
        const body = await response.json();
        if (body && body.error) message = String(body.error);
      } catch (_) {}
      toast(message);
      return;
    }
    toast('Команда принята');
    await pollState();
  } catch (_) {
    toast('Нет связи с устройством');
  }
}

// ------------------------------------------------------------------
// Фракции
// ------------------------------------------------------------------
function renderFractions() {
  const fraction = fractionFromStage(state.stage);
  const app = $('#app');
  if (!app) return;

  app.innerHTML = `
    <section class="section">
      <div class="card hero-card">
        <div class="hero-label">Текущая фракция</div>
        <div class="hero-value"><span id="fraction-name">${fraction.label}</span></div>
        <div class="hero-meta">
          <span class="badge badge-${fraction.key}">${STAGE_NAMES[state.stage] || '—'}</span>
        </div>
      </div>
    </section>

    <section class="section">
      <div class="section-title">Пороги автоматики</div>
      <div class="card">
        <div class="card-row">
          <div class="card-row-main">
            <div class="card-row-title">Головы → Тело</div>
            <div class="card-row-sub">78.5 °C или 50 мл</div>
          </div>
          <span class="card-row-value">авто</span>
        </div>
        <div class="card-row">
          <div class="card-row-main">
            <div class="card-row-title">Тело → Хвосты</div>
            <div class="card-row-sub">83.0 °C или 2000 мл</div>
          </div>
          <span class="card-row-value">авто</span>
        </div>
        <div class="card-row">
          <div class="card-row-main">
            <div class="card-row-title">Хвосты → Готово</div>
            <div class="card-row-sub">98.0 °C</div>
          </div>
          <span class="card-row-value">авто</span>
        </div>
      </div>
    </section>

    <section class="section">
      <div class="card">
        <div class="card-row">
          <div class="card-row-main">
            <div class="card-row-title">Собрано</div>
            <div class="card-row-sub">головы / тело / хвосты</div>
          </div>
          <span class="card-row-value" id="fraction-volume">0 / 0 / 0 мл</span>
        </div>
      </div>
      <p class="hint mt-8">
        Переключение фракций выполняет сама прошивка по температуре с поправкой
        на давление. Ручной выбор в этой версии не поддерживается: это
        предотвращает конфликт с автоматикой безопасности.
      </p>
    </section>
  `;

  updateFractionsLive();
}

function updateFractionsLive() {
  if (currentPage !== 'fractions') return;

  const fraction = fractionFromStage(state.stage);
  const name = $('#fraction-name');
  if (name) name.textContent = fraction.label;

  const volume = $('#fraction-volume');
  if (volume) {
    volume.textContent = `${Math.round(state.heads)} / ${Math.round(state.body)} / ${Math.round(state.tails)} мл`;
  }
}

// ------------------------------------------------------------------
// Настройки
// ------------------------------------------------------------------
function renderSettings() {
  const app = $('#app');
  if (!app) return;

  app.innerHTML = `
    <section class="section">
      <div class="section-title">Доступ</div>
      <div class="card">
        <div class="field">
          <label class="field-label" for="api-key">API-ключ (X-API-Key)</label>
          <input id="api-key" class="input" type="password" autocomplete="off"
                 placeholder="32 hex-символа">
        </div>
        <div class="btn-row">
          <button class="btn btn-primary" type="button" id="key-save">Сохранить</button>
          <button class="btn btn-secondary" type="button" id="key-generate">Сгенерировать</button>
        </div>
        <div class="btn-row mt-8">
          <button class="btn btn-secondary btn-block" type="button" id="key-clear">Забыть ключ</button>
        </div>
        <p class="hint mt-8">
          Ключ хранится только в localStorage этого браузера. Пока ключ не создан,
          устройство разрешает bootstrap-генерацию один раз.
        </p>
      </div>
    </section>

    <section class="section">
      <div class="section-title">Регулирование</div>
      <form id="control-form" class="card">
        <div class="field">
          <span class="field-label">Режим управления</span>
          <div class="segmented" id="ctrl-seg">
            <button type="button" data-val="0" class="active">PID</button>
            <button type="button" data-val="1">ШИМ / HYS</button>
          </div>
        </div>

        <div class="field">
          <span class="field-label">Устройство отбора</span>
          <div class="segmented" id="out-seg">
            <button type="button" data-val="0" class="active">Клапан</button>
            <button type="button" data-val="1">Насос</button>
          </div>
        </div>

        <div class="field">
          <label class="field-label" for="rate">Скорость отбора, мл/мин</label>
          <input id="rate" class="input" type="number" min="0" max="200" step="0.1" inputmode="decimal" placeholder="0.0">
        </div>

        <div class="field">
          <label class="field-label" for="hys">Гистерезис, °C</label>
          <input id="hys" class="input" type="number" min="0" max="10" step="0.1" inputmode="decimal" placeholder="0.5">
        </div>

        <div class="field">
          <label class="field-label" for="steps">Шагов на мл</label>
          <input id="steps" class="input" type="number" min="1" max="100000" step="1" inputmode="numeric" placeholder="320">
        </div>

        <button class="btn btn-primary btn-block" type="submit">Сохранить настройки</button>
      </form>
    </section>

    <section class="section">
      <div class="section-title">Приложение</div>
      <div class="card">
        <div class="card-row">
          <div class="card-row-main">
            <div class="card-row-title">Установить на телефон</div>
            <div class="card-row-sub" id="install-hint">HTTPS и доверенный сертификат</div>
          </div>
          <button class="btn btn-secondary" type="button" id="install-settings">Установить</button>
        </div>
        <div class="card-row">
          <div class="card-row-main">
            <div class="card-row-title">Версия интерфейса</div>
            <div class="card-row-sub mono" id="app-version">${APP_VERSION}</div>
          </div>
          <span class="card-row-value" id="fw-version">—</span>
        </div>
      </div>
    </section>
  `;

  const keyInput = $('#api-key');
  if (keyInput) keyInput.value = apiKey();

  $('#key-save')?.addEventListener('click', () => {
    setApiKey((keyInput?.value || '').trim());
    toast('Ключ сохранён');
  });

  $('#key-clear')?.addEventListener('click', () => {
    setApiKey('');
    if (keyInput) keyInput.value = '';
    toast('Ключ удалён из браузера');
  });

  $('#key-generate')?.addEventListener('click', generateKey);

  $('#install-settings')?.addEventListener('click', promptInstall);

  bindSegmented('#ctrl-seg');
  bindSegmented('#out-seg');

  $('#control-form')?.addEventListener('submit', async (event) => {
    event.preventDefault();
    const body = {
      ctrl_mode: Number($('#ctrl-seg .active')?.dataset.val || 0),
      out_dev: Number($('#out-seg .active')?.dataset.val || 0),
      rate_mlmin: Number($('#rate')?.value || 0),
      hys: Number($('#hys')?.value || 0),
      steps_per_ml: Number($('#steps')?.value || 1)
    };
    try {
      const response = await postJson('/api/control', body);
      if (!response.ok) throw new Error(String(response.status));
      toast('Настройки сохранены');
    } catch (_) {
      toast('Не удалось сохранить настройки');
    }
  });

  loadControlSettings();
  loadFirmwareVersion();
  updateInstallHint();
}

function bindSegmented(selector) {
  const segmented = $(selector);
  if (!segmented) return;
  segmented.addEventListener('click', (event) => {
    const button = event.target.closest('button');
    if (!button) return;
    $$('button', segmented).forEach((b) => b.classList.remove('active'));
    button.classList.add('active');
  });
}

async function generateKey() {
  const status = $('#key-status');
  if (status) status.textContent = 'Запрашиваю ключ…';
  try {
    const response = await postJson('/api/config/generate-key');
    if (!response.ok) throw new Error(String(response.status));
    const data = await response.json();
    if (!data.api_key) throw new Error('no key');
    setApiKey(data.api_key);
    const input = $('#api-key');
    if (input) input.value = data.api_key;
    toast('Ключ создан и сохранён');
  } catch (_) {
    toast('Не удалось создать ключ');
  }
}

async function loadControlSettings() {
  try {
    const response = await api('/api/control');
    if (!response.ok) return;
    const data = await response.json();

    const setActive = (selector, value) => {
      $$(selector + ' button').forEach((button) => {
        button.classList.toggle('active', Number(button.dataset.val) === Number(value));
      });
    };
    setActive('#ctrl-seg', data.ctrl_mode);
    setActive('#out-seg', data.out_dev);

    if ($('#rate')) $('#rate').value = Number(data.rate_mlmin || 0).toFixed(1);
    if ($('#hys')) $('#hys').value = Number(data.hys || 0).toFixed(1);
    if ($('#steps')) $('#steps').value = String(data.steps_per_ml || 1);
  } catch (_) {
    toast('Настройки регулирования недоступны');
  }
}

async function loadFirmwareVersion() {
  try {
    const response = await fetch('/api/health');
    if (!response.ok) return;
    const data = await response.json();
    const el = $('#fw-version');
    if (el && data.version) el.textContent = 'FW ' + data.version;
  } catch (_) {}
}

function updateInstallHint() {
  const hint = $('#install-hint');
  if (!hint) return;
  if (window.matchMedia('(display-mode: standalone)').matches) {
    hint.textContent = 'Уже установлено';
  } else if (location.protocol !== 'https:' && location.hostname !== 'localhost') {
    hint.textContent = 'Нужен HTTPS: откройте https://automatika.local';
  } else if (deferredPrompt) {
    hint.textContent = 'Готово к установке';
  } else {
    hint.textContent = 'Chrome: меню → Установить; Safari: Поделиться → На экран «Домой»';
  }
}

function promptInstall() {
  if (!deferredPrompt) {
    toast('Установка недоступна: откройте по HTTPS или добавьте вручную');
    return;
  }
  deferredPrompt.prompt();
  deferredPrompt.userChoice.finally(() => {
    deferredPrompt = null;
    updateInstallHint();
  });
}

// ------------------------------------------------------------------
// Журнал
// ------------------------------------------------------------------
async function renderLog() {
  const app = $('#app');
  if (!app) return;

  app.innerHTML = `
    <section class="section">
      <div class="row">
        <div class="section-title">Последние события</div>
        <button class="btn btn-secondary" type="button" id="log-refresh">Обновить</button>
      </div>
      <div id="log-list" class="log-list">
        <div class="empty">Загрузка…</div>
      </div>
    </section>
  `;

  $('#log-refresh')?.addEventListener('click', loadLog);
  loadLog();
}

async function loadLog() {
  const list = $('#log-list');
  if (!list) return;
  list.innerHTML = '<div class="empty">Загрузка…</div>';

  if (!apiKey()) {
    list.innerHTML = '<div class="empty">Укажите API-ключ в Настройках.</div>';
    return;
  }

  try {
    const response = await api('/api/events?limit=50');
    if (!response.ok) {
      list.innerHTML = '<div class="empty">Ошибка ' + response.status + '</div>';
      return;
    }
    const events = await response.json();
    if (!Array.isArray(events) || events.length === 0) {
      list.innerHTML = '<div class="empty">Событий нет</div>';
      return;
    }

    list.textContent = '';
    events.slice().reverse().forEach((event) => {
      const row = document.createElement('div');
      row.className = 'log-row';

      const message = document.createElement('div');
      message.className = 'log-msg';

      const type = document.createElement('span');
      const typeName = event.type === 2 ? 'error'
        : event.type === 1 ? 'warning'
        : event.type === 3 ? 'emergency' : 'info';
      type.className = 'log-type ' + typeName;
      type.textContent = typeName;
      message.appendChild(type);

      const text = document.createElement('span');
      text.textContent = event.msg || '';
      message.appendChild(text);
      row.appendChild(message);

      if (event.ts) {
        const time = document.createElement('div');
        time.className = 'log-time';
        time.textContent = new Date(event.ts * 1000).toLocaleString();
        row.appendChild(time);
      }

      list.appendChild(row);
    });
  } catch (_) {
    list.innerHTML = '<div class="empty">Нет связи с устройством.</div>';
  }
}

// ------------------------------------------------------------------
// Установка PWA
// ------------------------------------------------------------------
window.addEventListener('beforeinstallprompt', (event) => {
  event.preventDefault();
  deferredPrompt = event;
  const button = $('#install-btn');
  if (button) {
    button.hidden = false;
    button.classList.remove('hidden');
  }
  updateInstallHint();
});

window.addEventListener('appinstalled', () => {
  deferredPrompt = null;
  const button = $('#install-btn');
  if (button) {
    button.hidden = true;
    button.classList.add('hidden');
  }
  updateInstallHint();
  toast('Приложение установлено');
});

// ------------------------------------------------------------------
// Инициализация
// ------------------------------------------------------------------
function init() {
  $$('.tab-bar a').forEach((link) => {
    link.addEventListener('click', (event) => {
      event.preventDefault();
      navigate(link.dataset.page || 'dashboard');
    });
  });

  $('#install-btn')?.addEventListener('click', promptInstall);

  window.addEventListener('popstate', router);

  document.addEventListener('click', (event) => {
    const button = event.target.closest('[data-action]');
    if (button) runAction(button.dataset.action);
  });

  window.addEventListener('online', () => { setStatus(true); connectWS(); });
  window.addEventListener('offline', () => setStatus(false));

  window.addEventListener('resize', () => {
    if (currentPage === 'dashboard') initChart();
  });

  if ('serviceWorker' in navigator) {
    window.addEventListener('load', () => {
      navigator.serviceWorker.register('/sw.js').catch(() => {});
    });
  }

  router();
  connectWS();
  pollState();
  setInterval(pollState, 4000);

  // Обновляем производные значения (объём, фракция) на открытой странице.
  setInterval(() => {
    if (document.hidden) return;
    if (currentPage === 'fractions') updateFractionsLive();
    else renderLiveValues();
  }, 1000);
}

document.addEventListener('DOMContentLoaded', init);
