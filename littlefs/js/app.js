// Automatika PWA — glass UI.
//
// Работает только с реальными эндпоинтами прошивки:
//   GET  /api/state, /api/control, /api/health, /api/events
//   POST /api/start, /api/pause, /api/stop, /api/power, /api/safety,
//        /api/control, /api/config/generate-key
// Телеметрия — /ws/telemetry. Никаких несуществующих /api/fraction
// и /api/sensors/* здесь нет.

import {
  MENU,
  groupForPage,
  pageTitle,
  renderTabBar,
  renderSubMenu
} from './menu.js';
import { initTabSwipe, initEdgeBack } from './tab-swipe.js';
import { initPullToRefresh } from './pull-refresh.js';
import { attachSwipeAction } from './swipe-delete.js';
import { haptic } from './gestures.js';
import { updateSession, updateMinimizeState } from './session-widget.js';

const $ = (sel, root = document) => root.querySelector(sel);
const $$ = (sel, root = document) => Array.from(root.querySelectorAll(sel));

const STORAGE_KEY = 'automatika.apiKey';
const APP_VERSION = 'V0.5-alfa';

const STAGE_NAMES = [
  'Ожидание', 'Нагрев', 'Стабилизация', 'Головы', 'Тело',
  'Хвосты', 'Охлаждение', 'Готово', 'Проверка', 'Прогрев',
  'Работа', 'Ошибка'
];

const state = {
  tc: NaN,
  ts: NaN,
  power: 0,
  powerAuto: 0,
  powerManual: false,
  regulator: 0,
  regulatorReady: false,
  regulatorGpio: -1,
  pwmFreq: 0,
  pwmResolution: 0,
  pwmInverted: false,
  analogRange: 0,
  analogMin: 0,
  analogMax: 100,
  ssvrSqrt: true,
  regulatorError: '',
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
  estopHealthy: true,
  service: false,
  levelRequiredFault: false,
  online: false
};

let ws = null;
let wsRetry = null;
let chart = null;
let chartData = [];
let currentPage = 'dashboard';
let powerDragging = false;
let deferredPrompt = null;

const selectionState = {
  stage: 3,
  tFix: 0,
  openTime: 0,
  closeTime: 0,
  countdown: 0,
  ethanolMl: 0,
  ethanolPercent: 0,
  valveOpen: false,
  enabled: false,
  active: false
};

const paramDefs = {};

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

async function connectWS() {
  if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) return;

  let protocols = [];
  try {
    const tokenResponse = await api('/api/ws-token');
    if (tokenResponse.ok) {
      const tokenData = await tokenResponse.json();
      if (tokenData && tokenData.token) {
        protocols = ['automatika', tokenData.token];
      }
    }
  } catch (_) { /* ниже попробуем без subprotocol */ }

  try {
    ws = protocols.length
      ? new WebSocket(wsUrl(), protocols)
      : new WebSocket(wsUrl());
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
    const response = await api('/api/state');
    if (!response.ok) return;
    const data = await response.json();
    if (typeof data.tc !== 'undefined') state.tc = data.cube;
    if (typeof data.col !== 'undefined') state.ts = data.col;
    if (typeof data.power !== 'undefined') state.power = data.power;
    if (typeof data.power_auto !== 'undefined') state.powerAuto = data.power_auto;
    if (typeof data.power_manual !== 'undefined') state.powerManual = !!data.power_manual;
    if (typeof data.regulator !== 'undefined') state.regulator = data.regulator;
    if (typeof data.regulator_ready !== 'undefined') state.regulatorReady = !!data.regulator_ready;
    if (typeof data.regulator_gpio !== 'undefined') state.regulatorGpio = data.regulator_gpio;
    if (typeof data.pwm_freq_hz !== 'undefined') state.pwmFreq = data.pwm_freq_hz;
    if (typeof data.pwm_resolution !== 'undefined') state.pwmResolution = data.pwm_resolution;
    if (typeof data.pwm_inverted !== 'undefined') state.pwmInverted = !!data.pwm_inverted;
    if (typeof data.analog_range !== 'undefined') state.analogRange = data.analog_range;
    if (typeof data.analog_min_pct !== 'undefined') state.analogMin = data.analog_min_pct;
    if (typeof data.analog_max_pct !== 'undefined') state.analogMax = data.analog_max_pct;
    if (typeof data.ssvr_power_sqrt !== 'undefined') state.ssvrSqrt = !!data.ssvr_power_sqrt;
    if (typeof data.service !== 'undefined') state.service = !!data.service;
    if (typeof data.estop_healthy !== 'undefined') state.estopHealthy = !!data.estop_healthy;
    if (typeof data.level_required_fault !== 'undefined') state.levelRequiredFault = !!data.level_required_fault;
    if (typeof data.regulator_error !== 'undefined') state.regulatorError = data.regulator_error;
    if (typeof data.stage !== 'undefined') state.stage = data.stage;
    if (typeof data.running !== 'undefined') state.running = data.running;
    if (typeof data.paused !== 'undefined') state.paused = data.paused;
    if (typeof data.heads !== 'undefined') state.heads = data.heads;
    if (typeof data.body !== 'undefined') state.body = data.body;
    if (typeof data.tails !== 'undefined') state.tails = data.tails;
    if (typeof data.safety !== 'undefined') state.safety = data.safety;
    if (typeof data.estop !== 'undefined') state.estop = data.estop;
    updateSession({
      running: !!state.running,
      visible: currentPage === 'dashboard',
      stage: STAGE_NAMES[state.stage] || '—',
      tSteam: Number(state.ts),
      countdown: selectionState.countdown
    });
    updateMinimizeState(currentPage === 'dashboard' ? 'dashboard' : 'group',
                        state.running);
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
  dashboard: { render: renderDashboard },
  fraction: { render: renderFraction },
  profiles: { render: renderProfiles },
  calibration: { render: renderCalibration },
  autotune: { render: renderAutotune },
  sensors: { render: renderSensors },
  selection: { render: renderSelection },
  'selection-params': { render: renderSelectionParams },
  log: { render: renderLog },
  actuators: { render: renderActuators },
  health: { render: renderHealth },
  network: { render: renderNetwork },
  ota: { render: renderOta },
  api: { render: renderApi },
  lang: { render: renderLang },
  about: { render: renderAbout }
};

function currentRoute() {
  const params = new URLSearchParams(location.search);
  const page = params.get('page');
  const tab = params.get('tab');
  if (page && PAGES[page]) return { type: 'page', value: page };
  if (tab && MENU[tab]) return { type: 'group', value: tab };
  return { type: 'page', value: 'dashboard' };
}

function setHistory(params) {
  const url = new URL(location.href);
  url.search = '';
  url.hash = '';
  for (const [key, value] of Object.entries(params)) {
    if (value) url.searchParams.set(key, value);
  }
  history.pushState(params, '', url.toString());
}

function showPage(page, push = true) {
  if (!PAGES[page]) page = 'dashboard';
  currentPage = page;
  updateMinimizeState(page, state.running);
  const group = groupForPage(page);

  if (push) setHistory({ page });
  renderTabBar(group);
  const title = $('#page-title');
  if (title) title.textContent = pageTitle(page);

  chart = null;
  if (uplot) {
    uplot.destroy();
    uplot = null;
  }
  PAGES[page].render();
  renderLiveValues();
}

function showGroup(group, push = true) {
  if (!MENU[group]) group = 'work';
  currentPage = 'group:' + group;
  updateMinimizeState('group', state.running);

  if (push) setHistory({ tab: group });
  renderTabBar(group);
  const title = $('#page-title');
  if (title) title.textContent = MENU[group].label;

  const app = $('#app');
  if (app) app.innerHTML = renderSubMenu(group);
  renderLiveValues();
}

function router() {
  const route = currentRoute();
  if (route.type === 'group') showGroup(route.value, false);
  else showPage(route.value, false);
}

function currentGroup() {
  if (currentPage.startsWith('group:')) return currentPage.slice(6);
  return groupForPage(currentPage);
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
        <div class="row mt-8">
          <span id="power-mode" class="card-row-sub">Авто</span>
          <button class="btn btn-secondary" type="button" id="power-auto">Авто</button>
        </div>
        <p id="power-hint" class="hint mt-8 hidden"></p>
      </div>
    </section>

    <section class="section">
      <div class="card">
        <div class="card-row">
          <span class="card-row-title">ОТБОР · Периодический отбор</span>
          <span id="sel-stage" class="badge badge-idle">—</span>
        </div>
        <div class="card-row">
          <span class="card-row-title">Клапан</span>
          <div class="valve-inline">
            <span id="valve-dot" class="indicator red"></span>
            <span id="valve-text" class="card-row-value">—</span>
          </div>
        </div>
        <div class="card-row clickable" data-param="t_fix">
          <span class="card-row-title">T фиксации</span>
          <span id="p-tfix" class="card-row-value">—</span>
        </div>
        <div class="card-row clickable" data-param="open_time">
          <span class="card-row-title">Время открыт</span>
          <span id="p-open" class="card-row-value">—</span>
        </div>
        <div class="card-row">
          <span class="card-row-title" id="p-countdown-label">До закрытия</span>
          <span id="p-countdown" class="card-row-value">—</span>
        </div>
        <div class="card-row clickable" data-param="close_time">
          <span class="card-row-title">Время закрыт</span>
          <span id="p-close" class="card-row-value">—</span>
        </div>
        <div class="card-row clickable" data-param="hysteresis">
          <span class="card-row-title">Гистерезис</span>
          <span id="p-hyst" class="card-row-value">—</span>
        </div>
        <div class="card-row clickable" data-param="decrement">
          <span class="card-row-title">Декремент</span>
          <span id="p-decr" class="card-row-value">—</span>
        </div>
        <div class="card-row clickable" data-param="reflux">
          <span class="card-row-title">Флегмовое число</span>
          <span id="p-reflux" class="card-row-value">—</span>
        </div>
        <div class="card-row clickable" data-param="setpoint">
          <span class="card-row-title">Уставка</span>
          <span id="p-setpoint" class="card-row-value">—</span>
        </div>
        <div class="card-row clickable" data-param="min_open">
          <span class="card-row-title">Мин. открыт</span>
          <span id="p-min-open" class="card-row-value">—</span>
        </div>
        <div class="card-row clickable" data-param="heads_timer">
          <span class="card-row-title">Таймер голов</span>
          <span id="p-heads-timer" class="card-row-value">—</span>
        </div>
        <div class="card-row clickable" data-param="tails_end">
          <span class="card-row-title">Конец хвостов</span>
          <span id="p-tails-end" class="card-row-value">—</span>
        </div>
        <div class="card-row">
          <span class="card-row-title">Остаток спирта</span>
          <span id="p-ethanol" class="card-row-value">—</span>
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
      <div class="btn-row mt-8" id="minimize-row" hidden>
        <button class="btn btn-secondary btn-block" type="button" id="minimize-btn">
          Свернуть (процесс продолжит работу)
        </button>
      </div>
      <div class="btn-row mt-8">
        <button class="btn btn-secondary btn-block" type="button" data-action="safety">
          Сброс аварии
        </button>
      </div>
    </section>
  `;

  $('#minimize-btn')?.addEventListener('click', () => {
    haptic('light');
    updateMinimizeState('group', true);
    showGroup('prepare');
  });

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
        const response = await postJson('/api/power', {
          power: Number(slider.value)
        });
        const data = await response.json().catch(() => ({}));
        if (!response.ok) throw new Error(data.error || String(response.status));

        state.power = Number(data.power || 0);
        state.powerManual = Boolean(data.power_manual);
        state.regulatorReady = Boolean(data.regulator_ready);
        toast('Ручная мощность: ' + Math.round(state.power) + '%');
        renderLiveValues();
      } catch (_) {
        haptic('error');
        toast('Не удалось изменить мощность');
      }
    });
  }

  $('#power-auto')?.addEventListener('click', async () => {
    try {
      const response = await postJson('/api/power', { mode: 'auto' });
      const data = await response.json().catch(() => ({}));
      if (!response.ok) throw new Error(data.error || String(response.status));
      state.powerManual = false;
      toast('Мощность управляется рабочим режимом');
      pollState();
    } catch (_) {
      toast('Не удалось включить автоматический режим');
    }
  });

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

  const powerMode = $('#power-mode');
  if (powerMode) powerMode.textContent = state.powerManual ? 'Ручной' : 'Авто';

  const powerHint = $('#power-hint');
  if (powerHint) {
    const bad = state.regulator !== 0 && !state.regulatorReady;
    powerHint.classList.toggle('hidden', !bad);
    if (bad) {
      powerHint.textContent = state.regulatorGpio >= 0
        ? `Регулятор не готов (GPIO ${state.regulatorGpio}): ${state.regulatorError || 'проверьте параметры PWM'}. Выход 0.`
        : 'Регулятор не готов: PWM GPIO не задан. Выход 0.';
    }
  }

  const autoBtn = $('#power-auto');
  if (autoBtn) autoBtn.disabled = !state.powerManual;

  const powerSlider = $('#power-slider');
  const canManual = !!state.running || !!state.service;
  if (powerSlider) {
    powerSlider.disabled = !canManual;
    if (!canManual) {
      powerSlider.value = String(Math.round(state.power || 0));
    }
  }
  if (powerHint && !state.regulatorReady && state.regulator !== 0) {
    // сообщение о неготовом регуляторе уже выше
  } else if (powerHint && !canManual) {
    powerHint.classList.remove('hidden');
    powerHint.textContent = 'Ручная мощность доступна при запущенном процессе или в сервисном режиме.';
  }

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
  renderSelectionValues();

  const startBtn = document.querySelector('[data-action="start"]');
  if (startBtn) startBtn.disabled = !!state.running;
  const pauseBtn = document.querySelector('[data-action="pause"]');
  if (pauseBtn) pauseBtn.disabled = !state.running;

  const minimizeRow = $('#minimize-row');
  if (minimizeRow) minimizeRow.hidden = !state.running;
}

function updateStatusOnly() {
  setStatus(state.online);
}

function renderSelectionValues() {
  const enabled = selectionState.enabled;
  const stage = $('#sel-stage');
  if (stage) {
    const names = ['ГОЛОВЫ', 'ТЕЛО', 'ХВОСТЫ', 'ГОТОВО'];
    const classes = ['badge-heads', 'badge-body', 'badge-tails', 'badge-done'];
    stage.textContent = enabled ? (names[selectionState.stage] || '—') : 'выключен';
    stage.className = 'badge ' + (enabled ? (classes[selectionState.stage] || 'badge-idle') : 'badge-idle');
  }

  const dot = $('#valve-dot');
  const valve = $('#valve-text');
  if (dot && valve) {
    if (!enabled) {
      dot.className = 'indicator red';
      valve.textContent = '—';
    } else if (selectionState.valveOpen) {
      dot.className = 'indicator green pulse';
      valve.textContent = 'ОТКРЫТ';
    } else {
      dot.className = 'indicator red';
      valve.textContent = 'ЗАКРЫТ';
    }
  }

  const tfix = $('#p-tfix');
  if (tfix) tfix.textContent = enabled && selectionState.tFix > 0 ? selectionState.tFix.toFixed(2) + '°' : '—';

  const open = $('#p-open');
  if (open) open.textContent = enabled ? selectionState.openTime.toFixed(2) + ' с' : '—';

  const close = $('#p-close');
  if (close) close.textContent = enabled ? selectionState.closeTime.toFixed(2) + ' с' : '—';

  const countdown = $('#p-countdown');
  if (countdown) {
    countdown.textContent = enabled && selectionState.countdown > 0
      ? selectionState.countdown.toFixed(1) + ' с'
      : '—';
  }
  const countdownLabel = $('#p-countdown-label');
  if (countdownLabel) {
    countdownLabel.textContent = selectionState.valveOpen ? 'До закрытия' : 'До открытия';
  }

  const ethanol = $('#p-ethanol');
  if (ethanol) {
    ethanol.textContent = selectionState.ethanolMl > 0
      ? `${Math.round(selectionState.ethanolMl)} мл · ${Math.round(selectionState.ethanolPercent)}%`
      : '—';
  }

  renderParamValues();
}

const PARAM_FALLBACK = {
  t_fix: { label: 'T фиксации', unit: '°C', min: 0, max: 150, step: 0.1, value: 0, editable: true },
  open_time: { label: 'Время открыт', unit: 'с', min: 0, max: 3600, step: 0.1, value: 0, editable: true },
  close_time: { label: 'Время закрыт', unit: 'с', min: 0, max: 3600, step: 0.1, value: 0, editable: true },
  hysteresis: { label: 'Гистерезис', unit: '°C', min: 0, max: 20, step: 0.05, value: 0, editable: true },
  decrement: { label: 'Декремент', unit: '%', min: 0, max: 100, step: 1, value: 0, editable: true },
  reflux: { label: 'Флегмовое число', unit: ':1', min: 0, max: 10, step: 0.1, value: 0, editable: true },
  setpoint: { label: 'Уставка', unit: '°C', min: 30, max: 100, step: 0.5, value: 0, editable: false }
};

async function loadParamDefs() {
  try {
    const response = await fetch('/api/param');
    if (!response.ok) return;
    const data = await response.json();
    (data.params || []).forEach((p) => { paramDefs[p.id] = p; });
    if (currentPage === 'dashboard') renderParamValues();
  } catch (_) {}
}

function paramDef(name) {
  return paramDefs[name] || PARAM_FALLBACK[name] || null;
}

function renderParamValues() {
  const rows = {
    t_fix: ['#p-tfix', (v) => v.toFixed(2) + '°'],
    open_time: ['#p-open', (v) => v.toFixed(2) + ' с'],
    close_time: ['#p-close', (v) => v.toFixed(2) + ' с'],
    hysteresis: ['#p-hyst', (v) => v.toFixed(2) + '°'],
    decrement: ['#p-decr', (v) => Math.round(v) + '%'],
    reflux: ['#p-reflux', (v) => v.toFixed(1) + ':1'],
    setpoint: ['#p-setpoint', (v) => v.toFixed(1) + '°'],
    min_open: ['#p-min-open', (v) => v.toFixed(2) + ' с'],
    heads_timer: ['#p-heads-timer', (v) => Math.round(v / 60) + ' мин'],
    tails_end: ['#p-tails-end', (v) => v.toFixed(1) + '°']
  };

  for (const [name, [selector, format]] of Object.entries(rows)) {
    if (!paramDefs[name]) continue;
    const def = paramDef(name);
    const el = $(selector);
    if (def && el) el.textContent = format(Number(def.value || 0));
    const row = document.querySelector(`[data-param="${name}"]`);
    if (def && row) {
      row.classList.toggle('locked', !def.editable);
      row.classList.toggle('clickable', !!def.editable);
    }
  }

  const powerDef = paramDefs.power;
  const powerSlider = $('#power-slider');
  if (powerDef && powerSlider) {
    powerSlider.disabled = !powerDef.editable;
    powerSlider.title = powerDef.editable
      ? 'Мощность нагрева'
      : 'В PID-режиме мощность управляется регулятором';
  }
}

function openParamEditor(name) {
  const def = paramDef(name);
  if (!def) return;
  if (!def.editable) {
    haptic('error');
    toast('Нельзя изменить во время работы');
    return;
  }

  const initial = Number(def.value || 0);
  const entered = prompt(
    `${def.label} (${def.unit})\nДиапазон: ${def.min}…${def.max}`,
    String(initial)
  );
  if (entered === null || entered === '' || isNaN(+entered)) return;

  api('/api/param', {
    method: 'POST',
    body: JSON.stringify({ name, value: +entered })
  }).then(async (response) => {
    if (response.status === 403) {
      haptic('error');
      toast('Заблокировано во время работы');
      await loadParamDefs();
      return;
    }
    if (!response.ok) throw new Error(String(response.status));
    haptic('light');
    toast('Параметр сохранён');
    await loadParamDefs();
    pollSelection();
  }).catch(() => {
    haptic('error');
    toast('Не удалось сохранить параметр');
  });
}

window.editParam = openParamEditor;

document.addEventListener('click', (event) => {
  const row = event.target.closest('[data-param]');
  if (!row) return;
  openParamEditor(row.dataset.param);
});

async function pollSelection() {
  try {
    const response = await fetch('/api/selection/state');
    if (!response.ok) return;
    const data = await response.json();
    selectionState.stage = Number(data.stage ?? 3);
    selectionState.tFix = Number(data.t_fix || 0);
    selectionState.openTime = Number(data.open_time || 0);
    selectionState.closeTime = Number(data.close_time || 0);
    selectionState.countdown = Number(data.countdown || 0);
    selectionState.ethanolMl = Number(data.ethanol_ml || 0);
    selectionState.ethanolPercent = Number(data.ethanol_percent || 0);
    selectionState.valveOpen = !!data.valve_open;
    selectionState.enabled = !!data.enabled;
    selectionState.active = !!data.active;
    if (paramDefs.t_fix) paramDefs.t_fix.value = selectionState.tFix;
    if (paramDefs.open_time) paramDefs.open_time.value = selectionState.openTime;
    if (paramDefs.close_time) paramDefs.close_time.value = selectionState.closeTime;
    updateSession({
      running: !!state.running,
      visible: currentPage === 'dashboard',
      stage: ['ГОЛОВЫ', 'ТЕЛО', 'ХВОСТЫ', 'ГОТОВО'][selectionState.stage] || '—',
      tSteam: Number(state.ts),
      countdown: selectionState.countdown
    });
    if (currentPage === 'dashboard') renderSelectionValues();
  } catch (_) {}
}

// ------------------------------------------------------------------
// График
// ------------------------------------------------------------------
let uplot = null;

function initChart() {
  const canvas = $('#chart');
  if (!canvas) {
    chart = null;
    return;
  }

  const host = canvas.parentElement;
  if (uplot) {
    uplot.destroy();
    uplot = null;
  }

  if (window.uPlot && host) {
    canvas.hidden = true;
    const width = Math.max(280, Math.floor(host.clientWidth || 320));
    const data = [[], [], []];
    chartData.forEach((point, index) => {
      data[0].push(point.t || (Date.now() / 1000 - (chartData.length - index)));
      data[1].push(point.tc);
      data[2].push(point.ts);
    });

    uplot = new uPlot({
      width,
      height: 140,
      padding: [8, 8, 0, 0],
      legend: { show: false },
      cursor: { drag: { x: true, y: false }, points: { show: false } },
      scales: { x: { time: true }, y: { auto: true } },
      axes: [
        {
          stroke: cssVar('--text-secondary'),
          grid: { stroke: cssVar('--separator') },
          ticks: { show: false },
          font: '11px system-ui, sans-serif'
        },
        {
          stroke: cssVar('--text-secondary'),
          grid: { stroke: cssVar('--separator') },
          size: 40,
          font: '11px system-ui, sans-serif'
        }
      ],
      series: [
        {},
        { label: 'куб', stroke: cssVar('--accent') || '#007aff', width: 2 },
        { label: 'пар', stroke: cssVar('--orange') || '#ff9500', width: 2 }
      ]
    }, data, host);

    chart = { uplot };
    return;
  }

  canvas.hidden = false;
  const rect = host.getBoundingClientRect();
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
  chartData.push({ t: Date.now() / 1000, tc: state.tc, ts: state.ts });
  if (chartData.length > 300) chartData.shift();

  if (uplot) {
    uplot.setData([
      chartData.map((point) => point.t),
      chartData.map((point) => point.tc),
      chartData.map((point) => point.ts)
    ]);
  } else {
    drawChart();
  }
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
    toast('Сначала задайте API-ключ: Система → API-ключ');
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
      haptic('error');
      toast(message);
      return;
    }
    haptic('light');
    toast('Команда принята');
    await pollState();
  } catch (_) {
    haptic('error');
    toast('Нет связи с устройством');
  }
}

// ------------------------------------------------------------------
// Фракции
// ------------------------------------------------------------------
function renderFraction() {
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
  if (currentPage !== 'fraction') return;

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
function renderCalibration() {
  const app = $('#app');
  if (!app) return;

  app.innerHTML = `
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

        <div class="card-row">
          <div class="card-row-main">
            <div class="card-row-title">Многоуровневый адаптивный PID</div>
            <div class="card-row-sub">L1 фазы + L2 RLS + L3 откат; только после verified</div>
          </div>
          <input id="ad-pid" type="checkbox">
        </div>

        <div class="field">
          <span class="field-label">Внешний регулятор мощности</span>
          <div class="segmented" id="reg-seg">
            <button type="button" data-val="0" class="active">Нет</button>
            <button type="button" data-val="1">KVIC</button>
            <button type="button" data-val="2">РМВ-К</button>
            <button type="button" data-val="3">SEM</button>
            <button type="button" data-val="4">SSR</button>
            <button type="button" data-val="5">Finglai</button>
          </div>
          <p class="hint mt-8">
            UART-регуляторы подключайте только через оптронную развязку.
            Земли контроллера и силовой части не соединяйте.
          </p>
          <p id="reg-status" class="hint"></p>
        </div>

        <div class="field">
          <label class="field-label" for="reg-baud">Скорость UART, бод</label>
          <input id="reg-baud" class="input" type="number" min="1200" max="115200" step="1" inputmode="numeric" value="9600">
        </div>

        <div class="field">
          <label class="field-label" for="reg-gpio">PWM GPIO (ESP32-S3: 0–21, 26–48)</label>
          <input id="reg-gpio" class="input" type="number" min="-1" max="48" step="1" inputmode="numeric" value="-1">
          <p class="hint mt-8">-1 = не задан. GPIO 22–25 у ESP32-S3 отсутствуют; 26–37 обычно заняты flash/PSRAM.</p>
        </div>

        <div class="field">
          <label class="field-label" for="reg-freq">Частота PWM, Гц</label>
          <input id="reg-freq" class="input" type="number" min="1" max="20000" step="1" inputmode="numeric" value="1000">
          <p class="hint mt-8">5 кГц подходит не всем входам. Для AC SSR нужен zero-cross или иной режим.</p>
        </div>

        <div class="field">
          <label class="field-label" for="reg-res">Разрядность PWM, бит</label>
          <input id="reg-res" class="input" type="number" min="1" max="14" step="1" inputmode="numeric" value="10">
        </div>

        <div class="card-row">
          <div class="card-row-main">
            <div class="card-row-title">Инверсия PWM</div>
            <div class="card-row-sub">Активный низкий вход: 0 % = высокий уровень</div>
          </div>
          <input id="reg-inv" type="checkbox">
        </div>

        <div class="field">
          <label class="field-label" for="analog-range">Finglai SSVR: аналоговый диапазон</label>
          <select id="analog-range" class="input">
            <option value="0">0–5 В (CONT +, COM −)</option>
            <option value="1">0–10 В</option>
            <option value="2">0–3,3 В</option>
          </select>
        </div>

        <div class="field">
          <label class="field-label" for="analog-min">Калибровка 0 В: duty, %</label>
          <input id="analog-min" class="input" type="number" min="0" max="99" step="0.1" inputmode="decimal" value="0">
        </div>

        <div class="field">
          <label class="field-label" for="analog-max">Калибровка макс.: duty, %</label>
          <input id="analog-max" class="input" type="number" min="1" max="100" step="0.1" inputmode="decimal" value="100">
        </div>

        <div class="card-row">
          <div class="card-row-main">
            <div class="card-row-title">Finglai: процент = электрическая мощность</div>
            <div class="card-row-sub">Команда = 100·sqrt(P/100) при линейном RMS-напряжении SSVR</div>
          </div>
          <input id="ssvr-sqrt" type="checkbox" checked>
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
      <div class="section-title">Проверка дозатора</div>
      <div class="card">
        <p class="hint">
          Поставьте трубку в мерный цилиндр. После дозы замерьте объём:
          шаги = старые шаги × факт / 10. Сохраните новое значение выше.
        </p>
        <div class="btn-row mt-8">
          <button class="btn btn-secondary" type="button" data-dose="10">10 мл</button>
          <button class="btn btn-secondary" type="button" data-dose="50">50 мл</button>
        </div>
      </div>
    </section>
  `;

  $$('[data-dose]', app).forEach((button) => {
    button.addEventListener('click', async () => {
      const volume = Number(button.dataset.dose || 10);
      try {
        const response = await postJson('/api/dose', {
          volume_ml: volume,
          rate_mlmin: Number($('#rate')?.value || 60)
        });
        if (!response.ok) throw new Error(String(response.status));
        toast('Дозирование ' + volume + ' мл запущено');
      } catch (_) {
        toast('Не удалось запустить дозатор');
      }
    });
  });

  bindSegmented('#ctrl-seg');
  bindSegmented('#out-seg');
  bindSegmented('#reg-seg');

  $('#control-form')?.addEventListener('submit', async (event) => {
    event.preventDefault();
    const body = {
      ctrl_mode: Number($('#ctrl-seg .active')?.dataset.val || 0),
      out_dev: Number($('#out-seg .active')?.dataset.val || 0),
      regulator: Number($('#reg-seg .active')?.dataset.val || 0),
      reg_baud: Number($('#reg-baud')?.value || 9600),
      reg_gpio: Number($('#reg-gpio')?.value ?? -1),
      reg_freq: Number($('#reg-freq')?.value || 1000),
      reg_resolution: Number($('#reg-res')?.value || 10),
      reg_inverted: !!$('#reg-inv')?.checked,
      analog_range: Number($('#analog-range')?.value || 0),
      analog_min_pct: Number($('#analog-min')?.value || 0),
      analog_max_pct: Number($('#analog-max')?.value ?? 100),
      ssvr_power_sqrt: !!$('#ssvr-sqrt')?.checked,
      adaptive_pid: !!$('#ad-pid')?.checked,
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
}

async function renderProfiles() {
  const app = $('#app');
  if (!app) return;

  app.innerHTML = `
    <section class="section">
      <div class="section-title">Сохранённые профили</div>
      <div id="profile-list" class="card"><div class="empty">Загрузка…</div></div>
    </section>

    <section class="section">
      <div class="section-title">Создать / изменить</div>
      <form id="profile-form" class="card">
        <div class="field">
          <label class="field-label" for="profile-name">Имя профиля</label>
          <input id="profile-name" class="input" type="text" maxlength="31" placeholder="default">
        </div>
        <div class="field">
          <label class="field-label" for="profile-json">JSON профиля</label>
          <textarea id="profile-json" class="input profile-json" rows="7" spellcheck="false">{}</textarea>
        </div>
        <button class="btn btn-primary btn-block" type="submit">Сохранить профиль</button>
        <p class="hint mt-8">
          Профиль — JSON с уставками. Выбор профиля для процесса выполняется
          на устройстве; здесь можно создать, прочитать и удалить файл.
        </p>
      </form>
    </section>
  `;

  $('#profile-form')?.addEventListener('submit', async (event) => {
    event.preventDefault();
    const name = ($('#profile-name')?.value || '').trim();
    const raw = ($('#profile-json')?.value || '').trim();
    if (!name) { toast('Укажите имя профиля'); return; }
    let profile;
    try { profile = JSON.parse(raw || '{}'); } catch (_) {
      toast('JSON некорректен'); return;
    }
    if (!profile || typeof profile !== 'object' || Array.isArray(profile)) {
      toast('Профиль должен быть JSON-объектом'); return;
    }
    try {
      const response = await postJson('/api/profiles', { name, profile });
      if (!response.ok) throw new Error(String(response.status));
      toast('Профиль сохранён');
      loadProfiles();
    } catch (_) {
      toast('Не удалось сохранить профиль');
    }
  });

  loadProfiles();
}

async function loadProfiles() {
  const list = $('#profile-list');
  if (!list) return;
  list.innerHTML = '<div class="empty">Загрузка…</div>';

  try {
    const response = await api('/api/profiles');
    if (!response.ok) {
      const message = response.status === 401
        ? 'Нужен API-ключ: Система → API-ключ'
        : 'Ошибка ' + response.status;
      list.innerHTML = `<div class="empty">${message}</div>`;
      return;
    }
    const names = await response.json();
    if (!Array.isArray(names) || names.length === 0) {
      list.innerHTML = '<div class="empty">Профилей нет</div>';
      return;
    }

    list.textContent = '';
    names.forEach((name) => {
      const row = document.createElement('div');
      row.className = 'card-row';

      const main = document.createElement('div');
      main.className = 'card-row-main';
      const title = document.createElement('div');
      title.className = 'card-row-title';
      title.textContent = name;
      main.appendChild(title);

      const actions = document.createElement('div');
      actions.className = 'row';

      const load = document.createElement('button');
      load.className = 'btn btn-ghost';
      load.type = 'button';
      load.textContent = 'Открыть';
      load.addEventListener('click', async () => {
        try {
          const r = await api('/api/profiles?name=' + encodeURIComponent(name));
          if (!r.ok) throw new Error(String(r.status));
          const data = await r.json();
          const input = $('#profile-name');
          const json = $('#profile-json');
          if (input) input.value = name;
          if (json) json.value = JSON.stringify(data, null, 2);
          toast('Профиль загружен в редактор');
        } catch (_) {
          toast('Не удалось прочитать профиль');
        }
      });

      const remove = document.createElement('button');
      remove.className = 'btn btn-ghost text-danger';
      remove.type = 'button';
      remove.textContent = 'Удалить';
      remove.addEventListener('click', async () => {
        if (!window.confirm('Удалить профиль «' + name + '»?')) return;
        try {
          const r = await api('/api/profiles', {
            method: 'DELETE',
            body: JSON.stringify({ name })
          });
          if (!r.ok) throw new Error(String(r.status));
          toast('Профиль удалён');
          loadProfiles();
        } catch (_) {
          toast('Не удалось удалить профиль');
        }
      });

      actions.appendChild(load);
      actions.appendChild(remove);
      row.appendChild(main);
      row.appendChild(actions);
      list.appendChild(row);

      attachSwipeAction(row, {
        label: 'Удалить',
        onAction: async () => {
          try {
            const r = await api('/api/profiles', {
              method: 'DELETE',
              body: JSON.stringify({ name })
            });
            if (!r.ok) throw new Error(String(r.status));
            haptic('heavy');
            toast('Профиль удалён');
            return true;
          } catch (_) {
            haptic('error');
            toast('Не удалось удалить профиль');
            return false;
          }
        }
      });
    });
  } catch (_) {
    list.innerHTML = '<div class="empty">Нет связи с устройством</div>';
  }
}

function setSegment(selector, value) {
  $$(selector + ' button').forEach((button) => {
    button.classList.toggle('active', Number(button.dataset.val) === Number(value));
  });
}

function segmentValue(selector, fallback = 0) {
  const active = $(selector + ' .active');
  return active ? Number(active.dataset.val) : fallback;
}

async function renderSelection() {
  const app = $('#app');
  if (!app) return;

  app.innerHTML = `
    <section class="section">
      <div class="section-title">Отбор · вариант</div>
      <div class="card">
        <div class="card-row">
          <div class="card-row-main">
            <div class="card-row-title">Использовать Периодический отбор</div>
            <div class="card-row-sub">Заменяет старую логику фракций</div>
          </div>
          <input id="sel-enabled" type="checkbox">
        </div>

        <div class="card-row">
          <div class="card-row-main">
            <div class="card-row-title">Профиль режима</div>
            <div class="card-row-sub">open/close и устройство при старте процесса</div>
          </div>
          <input id="sel-auto-defaults" type="checkbox">
        </div>

        <div class="field mt-16">
          <span class="field-label">Устройство отбора</span>
          <div class="segmented" id="seg-device">
            <button type="button" data-val="0" class="active">Клапан</button>
            <button type="button" data-val="1">Насос</button>
          </div>
        </div>

        <div class="field">
          <span class="field-label">Режим переходов</span>
          <div class="segmented" id="seg-transition">
            <button type="button" data-val="0" class="active">Авто</button>
            <button type="button" data-val="1">Вручную</button>
          </div>
        </div>

        <div class="field">
          <span class="field-label">Режим отбора</span>
          <div class="segmented" id="seg-select">
            <button type="button" data-val="0" class="active">Периодический</button>
            <button type="button" data-val="1">Непрерывный</button>
          </div>
        </div>

        <div class="field">
          <span class="field-label">Алгоритм отбора</span>
          <div class="segmented" id="seg-algo">
            <button type="button" data-val="0" class="active">Классический</button>
            <button type="button" data-val="1">Тренд</button>
            <button type="button" data-val="2">Адаптивный</button>
          </div>
        </div>

        <div class="field">
          <span class="field-label">Датчик этапа</span>
          <div class="segmented" id="seg-sensor">
            <button type="button" data-val="0" class="active">T куба</button>
            <button type="button" data-val="1">T пара</button>
            <button type="button" data-val="2">T колонны</button>
          </div>
        </div>

        <button class="btn btn-primary btn-block mt-8" type="button" id="selection-save">
          Сохранить вариант
        </button>
      </div>
      <p class="hint">
        Периодический режим открывает клапан на open_time внутри периода.
        Непрерывный для клапана — постоянно открыт; для насоса — постоянная
        подача. Отбор не отключает E-STOP и safety.
      </p>
    </section>
  `;

  bindSegmented('#seg-device');
  bindSegmented('#seg-transition');
  bindSegmented('#seg-select');
  bindSegmented('#seg-algo');
  bindSegmented('#seg-sensor');

  try {
    const response = await fetch('/api/selection/config');
    if (response.ok) {
      const cfg = await response.json();
      $('#sel-enabled').checked = !!cfg.enabled;
      if ($('#sel-auto-defaults')) $('#sel-auto-defaults').checked = !!cfg.auto_mode_defaults;
      setSegment('#seg-device', cfg.out_device);
      setSegment('#seg-transition', cfg.transition_mode);
      setSegment('#seg-select', cfg.select_mode);
      setSegment('#seg-algo', cfg.algo);
      setSegment('#seg-sensor', cfg.sensor);
    }
  } catch (_) {}

  $('#selection-save')?.addEventListener('click', async () => {
    try {
      const response = await postJson('/api/selection/options', {
        enabled: !!$('#sel-enabled')?.checked,
        auto_mode_defaults: !!$('#sel-auto-defaults')?.checked,
        out_device: segmentValue('#seg-device'),
        transition_mode: segmentValue('#seg-transition'),
        select_mode: segmentValue('#seg-select'),
        algo: segmentValue('#seg-algo'),
        sensor: segmentValue('#seg-sensor')
      });
      if (!response.ok) throw new Error(String(response.status));
      toast('Вариант отбора сохранён');
      pollSelection();
    } catch (_) {
      toast('Не удалось сохранить вариант отбора');
    }
  });
}

async function renderSelectionParams() {
  const app = $('#app');
  if (!app) return;

  app.innerHTML = `
    <section class="section">
      <div class="section-title">Заливка куба</div>
      <div class="card">
        <div class="field">
          <label class="field-label" for="abv-volume">Начальный объём, мл</label>
          <input id="abv-volume" class="input" type="number" min="1" max="200000" step="1" value="20000">
        </div>
        <div class="field">
          <label class="field-label" for="abv-percent">Начальная крепость, %</label>
          <input id="abv-percent" class="input" type="number" min="0" max="100" step="0.1" value="40">
        </div>
        <button class="btn btn-secondary btn-block" type="button" id="abv-save">
          Сохранить заливку
        </button>
        <p class="hint mt-8">
          Остаток спирта оценивается по T куба, давлению и скорости отбора.
          Значения сбрасываются при старте процесса.
        </p>
      </div>
    </section>

    <section class="section">
      <div class="section-title">Головы</div>
      <div class="card">
        <div class="field">
          <label class="field-label" for="heads-timer">Таймер, мин (0 — без таймера)</label>
          <input id="heads-timer" class="input" type="number" min="0" step="1" value="60">
        </div>
        <div class="field">
          <label class="field-label" for="heads-open">Открыт, с</label>
          <input id="heads-open" class="input" type="number" min="0" step="0.1" value="2.0">
        </div>
        <div class="field">
          <label class="field-label" for="heads-close">Закрыт, с</label>
          <input id="heads-close" class="input" type="number" min="0" step="0.1" value="8.0">
        </div>
      </div>
    </section>

    <section class="section">
      <div class="section-title">Тело</div>
      <div class="card">
        <div class="field">
          <label class="field-label" for="body-open">Открыт, с</label>
          <input id="body-open" class="input" type="number" min="0" step="0.1" value="1.5">
        </div>
        <div class="field">
          <label class="field-label" for="body-close">Закрыт, с</label>
          <input id="body-close" class="input" type="number" min="0" step="0.1" value="8.5">
        </div>
        <div class="field">
          <label class="field-label" for="body-hyst">Гистерезис, °C</label>
          <input id="body-hyst" class="input" type="number" min="0" step="0.01" value="0.2">
        </div>
        <div class="field">
          <label class="field-label" for="body-decr">Декремент open_time, %</label>
          <input id="body-decr" class="input" type="number" min="0" max="100" step="1" value="10">
        </div>
        <div class="field">
          <label class="field-label" for="body-min">Минимальный open_time, с</label>
          <input id="body-min" class="input" type="number" min="0.1" step="0.1" value="0.1">
        </div>
        <div class="field">
          <label class="field-label" for="body-delay">Задержка фиксации T, мин</label>
          <input id="body-delay" class="input" type="number" min="0" step="1" value="0">
        </div>
      </div>
    </section>

    <section class="section">
      <div class="section-title">Хвосты</div>
      <div class="card">
        <div class="field">
          <label class="field-label" for="tails-end">Температура окончания, °C</label>
          <input id="tails-end" class="input" type="number" min="0" max="150" step="0.1" value="98">
        </div>
        <div class="field">
          <label class="field-label" for="tails-open">Открыт, с</label>
          <input id="tails-open" class="input" type="number" min="0" step="0.1" value="1.0">
        </div>
        <div class="field">
          <label class="field-label" for="tails-close">Закрыт, с</label>
          <input id="tails-close" class="input" type="number" min="0" step="0.1" value="9.0">
        </div>
      </div>
    </section>

    <section class="section">
      <div class="btn-row">
        <button class="btn btn-primary" type="button" id="sel-save">Сохранить</button>
        <button class="btn btn-secondary" type="button" id="sel-next">След. этап</button>
      </div>
      <p class="hint mt-8">
        «След. этап» работает только при включённом отборе и запущенном процессе.
      </p>
    </section>
  `;

  try {
    const response = await fetch('/api/selection/config');
    if (response.ok) {
      const cfg = await response.json();
      if ($('#heads-timer')) $('#heads-timer').value = Math.round((cfg.heads_timer_sec || 0) / 60);
      if ($('#heads-open')) $('#heads-open').value = Number(cfg.heads_open_time_sec || 0).toFixed(1);
      if ($('#heads-close')) $('#heads-close').value = Number(cfg.heads_close_time_sec || 0).toFixed(1);
      if ($('#body-open')) $('#body-open').value = Number(cfg.body_open_time_sec || 0).toFixed(1);
      if ($('#body-close')) $('#body-close').value = Number(cfg.body_close_time_sec || 0).toFixed(1);
      if ($('#body-hyst')) $('#body-hyst').value = Number(cfg.body_hysteresis_c || 0).toFixed(2);
      if ($('#body-decr')) $('#body-decr').value = Math.round(Number(cfg.body_decrement_percent || 0));
      if ($('#body-min')) $('#body-min').value = Number(cfg.body_min_open_time_sec || 0).toFixed(1);
      if ($('#body-delay')) $('#body-delay').value = Math.round((cfg.body_temp_fix_delay_sec || 0) / 60);
      if ($('#tails-end')) $('#tails-end').value = Number(cfg.tails_temperature_end || 0).toFixed(1);
      if ($('#tails-open')) $('#tails-open').value = Number(cfg.tails_open_time_sec || 0).toFixed(1);
      if ($('#tails-close')) $('#tails-close').value = Number(cfg.tails_close_time_sec || 0).toFixed(1);
    }
  } catch (_) {}

  try {
    const response = await fetch('/api/alcohol');
    if (response.ok) {
      const abv = await response.json();
      if ($('#abv-volume')) $('#abv-volume').value = Math.round(Number(abv.volume_start_ml || 20000));
      if ($('#abv-percent')) $('#abv-percent').value = Number(abv.abv_start_percent || 40).toFixed(1);
    }
  } catch (_) {}

  $('#abv-save')?.addEventListener('click', async () => {
    try {
      const response = await postJson('/api/alcohol', {
        volume_ml: Number($('#abv-volume')?.value || 0),
        abv_percent: Number($('#abv-percent')?.value || 0)
      });
      if (!response.ok) throw new Error(String(response.status));
      toast('Заливка сохранена, расчёт остатка сброшен');
      pollSelection();
    } catch (_) {
      toast('Не удалось сохранить заливку');
    }
  });

  $('#sel-save')?.addEventListener('click', async () => {
    try {
      const response = await postJson('/api/selection/config', {
        heads_timer_sec: Number($('#heads-timer')?.value || 0) * 60,
        heads_open_time_sec: Number($('#heads-open')?.value || 0),
        heads_close_time_sec: Number($('#heads-close')?.value || 0),
        body_open_time_sec: Number($('#body-open')?.value || 0),
        body_close_time_sec: Number($('#body-close')?.value || 0),
        body_hysteresis_c: Number($('#body-hyst')?.value || 0),
        body_decrement_percent: Number($('#body-decr')?.value || 0),
        body_min_open_time_sec: Number($('#body-min')?.value || 0.1),
        body_temp_fix_delay_sec: Number($('#body-delay')?.value || 0) * 60,
        tails_temperature_end: Number($('#tails-end')?.value || 98),
        tails_open_time_sec: Number($('#tails-open')?.value || 0),
        tails_close_time_sec: Number($('#tails-close')?.value || 0)
      });
      if (!response.ok) throw new Error(String(response.status));
      toast('Параметры отбора сохранены');
    } catch (_) {
      toast('Не удалось сохранить параметры отбора');
    }
  });

  $('#sel-next')?.addEventListener('click', async () => {
    try {
      const response = await postJson('/api/selection/next', {});
      if (!response.ok) throw new Error(String(response.status));
      toast('Переход на следующий этап');
      pollSelection();
    } catch (_) {
      toast('Переход недоступен');
    }
  });
}

async function renderAutotune() {
  const app = $('#app');
  if (!app) return;

  app.innerHTML = `
    <section class="section">
      <div class="card">
        <div class="card-row">
          <div class="card-row-main">
            <div class="card-row-title">Статус</div>
            <div class="card-row-sub" id="autotune-status">Загрузка…</div>
          </div>
          <span class="card-row-value" id="autotune-values">Kp — / Ki — / Kd —</span>
        </div>
        <div class="field mt-16">
          <label class="field-label" for="autotune-target">Уставка куба, °C</label>
          <input id="autotune-target" class="input" type="number" min="20" max="120" step="0.1" value="80">
        </div>
        <div class="btn-row">
          <button class="btn btn-primary" type="button" id="autotune-start">Запустить</button>
          <button class="btn btn-secondary" type="button" id="autotune-stop">Остановить</button>
        </div>
        <p class="hint mt-8">
          Автотюн занимает 30–60 минут, требует заполненного куба и работающего
          охлаждения. При E-STOP или защите отменяется.
        </p>
      </div>
    </section>
  `;

  const refresh = async () => {
    try {
      const response = await api('/api/autotune');
      if (!response.ok) {
        $('#autotune-status').textContent = 'Нужен API-ключ';
        return;
      }
      const data = await response.json();
      const status = data.running ? 'Выполняется' : (data.done ? 'Готово' : 'Ожидание');
      $('#autotune-status').textContent = status;
      $('#autotune-values').textContent =
        `Kp ${Number(data.kp || 0).toFixed(3)} / Ki ${Number(data.ki || 0).toFixed(4)} / Kd ${Number(data.kd || 0).toFixed(3)}`;
    } catch (_) {}
  };

  $('#autotune-start')?.addEventListener('click', async () => {
    const target = Number($('#autotune-target')?.value || 80);
    try {
      const response = await postJson('/api/autotune', { action: 'start', target });
      if (!response.ok) throw new Error(String(response.status));
      toast('Автотюн запущен');
      refresh();
    } catch (_) { toast('Не удалось запустить автотюн'); }
  });

  $('#autotune-stop')?.addEventListener('click', async () => {
    try {
      const response = await postJson('/api/autotune', { action: 'stop' });
      if (!response.ok) throw new Error(String(response.status));
      toast('Автотюн остановлен');
      refresh();
    } catch (_) { toast('Не удалось остановить автотюн'); }
  });

  refresh();
  setInterval(() => {
    if (currentPage === 'autotune' && !document.hidden) refresh();
  }, 3000);
}

async function renderSensors() {
  const app = $('#app');
  if (!app) return;

  app.innerHTML = `
    <section class="section">
      <div id="sensor-list" class="card"><div class="empty">Загрузка…</div></div>
      <p class="hint mt-8">
        Значения приходят из /api/env и /api/state. Если датчик не
        установлен, его поля будут нулевыми или present:0.
      </p>
    </section>
  `;

  try {
    const [envResponse, stateResponse] = await Promise.all([
      fetch('/api/env'), fetch('/api/state')
    ]);
    const env = envResponse.ok ? await envResponse.json() : {};
    const st = stateResponse.ok ? await stateResponse.json() : {};
    const baro = env.baro || {};
    const flow = env.flow || {};
    const column = env.column || {};

    const rows = [
      ['Куб', Number(st.cube || 0).toFixed(1) + ' °C'],
      ['Пар / царга', Number(st.col || 0).toFixed(1) + ' °C'],
      ['Давление', Number(baro.hpa || 0).toFixed(1) + ' гПа'],
      ['Кипение', Number(baro.boiling_c || 0).toFixed(1) + ' °C'],
      ['Колонна', Number(column.kpa || 0).toFixed(2) + ' кПа'],
      ['Проток', Number(flow.lpm || 0).toFixed(2) + ' л/мин'],
      ['Фракция', env.fraction || '—']
    ];

    const list = $('#sensor-list');
    list.innerHTML = rows.map(([label, value]) => `
      <div class="card-row">
        <span class="card-row-title">${label}</span>
        <span class="card-row-value">${value}</span>
      </div>
    `).join('');
  } catch (_) {
    $('#sensor-list').innerHTML = '<div class="empty">Нет связи с устройством</div>';
  }
}

async function renderActuators() {
  const app = $('#app');
  if (!app) return;

  app.innerHTML = `
    <section class="section">
      <div class="card">
        <div class="card-row">
          <div class="card-row-main">
            <div class="card-row-title">Сервисный режим</div>
            <div class="card-row-sub" id="service-status">Проверка…</div>
          </div>
          <button class="btn btn-secondary" type="button" id="service-toggle">Включить</button>
        </div>
      </div>
      <p class="hint">
        Прямые команды приводам выполняются только в сервисном режиме.
        Действия опасны: подтверждайте каждое.
      </p>
    </section>

    <section class="section">
      <div class="card">
        <div class="btn-row">
          <button class="btn btn-danger" type="button" data-act="heater_on">Нагрев 20 % / 5 с</button>
          <button class="btn btn-secondary" type="button" data-act="heater_off">Нагрев off</button>
        </div>
        <div class="btn-row mt-8">
          <button class="btn btn-secondary" type="button" data-act="valve_open">Клапан open</button>
          <button class="btn btn-secondary" type="button" data-act="valve_close">Клапан close</button>
        </div>
        <div class="btn-row mt-8">
          <button class="btn btn-secondary" type="button" data-act="pump_10">Насос 10 мл</button>
          <button class="btn btn-secondary" type="button" data-act="relay_on">Реле 0 on</button>
          <button class="btn btn-secondary" type="button" data-act="relay_off">Реле 0 off</button>
        </div>
      </div>
    </section>
  `;

  let service = false;

  const refresh = async () => {
    try {
      const response = await api('/api/diag/status');
      if (!response.ok) {
        $('#service-status').textContent = 'Нужен API-ключ';
        return;
      }
      const data = await response.json();
      service = !!data.service;
      $('#service-status').textContent = service ? 'Включён' : 'Выключен';
      $('#service-toggle').textContent = service ? 'Выключить' : 'Включить';
    } catch (_) {}
  };

  $('#service-toggle')?.addEventListener('click', async () => {
    try {
      const response = await postJson('/api/diag/service', { enabled: !service });
      if (!response.ok) throw new Error(String(response.status));
      service = !service;
      toast(service ? 'Сервисный режим включён' : 'Сервисный режим выключен');
      refresh();
    } catch (_) { toast('Не удалось переключить режим'); }
  });

  $$('[data-act]', app).forEach((button) => {
    button.addEventListener('click', async () => {
      if (!service) { toast('Сначала включите сервисный режим'); return; }
      const action = button.dataset.act;
      if (!window.confirm('Выполнить «' + button.textContent.trim() + '»?')) return;

      const commands = {
        heater_on: { actuator: 'heater', action: 'on', pct: 20, seconds: 5 },
        heater_off: { actuator: 'heater', action: 'off' },
        valve_open: { actuator: 'valve', action: 'open' },
        valve_close: { actuator: 'valve', action: 'close' },
        pump_10: { actuator: 'pump', action: 'dose', ml: 10, rate: 60 },
        relay_on: { actuator: 'relay', action: 'set', ch: 0, state: 1 },
        relay_off: { actuator: 'relay', action: 'set', ch: 0, state: 0 }
      };
      try {
        const response = await postJson('/api/diag/actuators', commands[action]);
        if (!response.ok) throw new Error(String(response.status));
        toast('Команда принята');
      } catch (_) { toast('Команда отклонена'); }
    });
  });

  refresh();
}

async function renderHealth() {
  const app = $('#app');
  if (!app) return;

  app.innerHTML = `<section class="section"><div id="health-list" class="card"><div class="empty">Загрузка…</div></div></section>`;

  try {
    const response = await fetch('/api/health');
    if (!response.ok) throw new Error(String(response.status));
    const data = await response.json();
    const rows = [
      ['Уровень', data.level || '—'],
      ['Причина', data.reason || '—'],
      ['Версия', data.version || '—'],
      ['Uptime', `${Math.floor(Number(data.uptime_s || 0) / 60)} мин`],
      ['Free heap', `${Math.round(Number(data.free_heap || 0) / 1024)} КБ`],
      ['Min heap', `${Math.round(Number(data.min_free_heap || 0) / 1024)} КБ`],
      ['PSRAM', `${Math.round(Number(data.free_psram || 0) / 1024)} КБ`],
      ['Wi-Fi RSSI', `${data.wifi_rssi || 0} dBm`],
      ['SD', data.sd ? 'есть' : 'нет'],
      ['Датчики', String(data.sensors || 0)],
      ['Safety', data.safety ? 'ok' : 'сработала'],
      ['E-STOP', data.estop ? 'активен' : 'нет']
    ];
    $('#health-list').innerHTML = rows.map(([label, value]) => `
      <div class="card-row">
        <span class="card-row-title">${label}</span>
        <span class="card-row-value">${value}</span>
      </div>
    `).join('');
  } catch (_) {
    $('#health-list').innerHTML = '<div class="empty">Нет связи с устройством</div>';
  }
}

async function renderNetwork() {
  const app = $('#app');
  if (!app) return;

  app.innerHTML = `
    <section class="section">
      <div class="card">
        <div class="card-row">
          <div class="card-row-main">
            <div class="card-row-title">Wi-Fi</div>
            <div class="card-row-sub" id="wifi-status">Загрузка…</div>
          </div>
        </div>
      </div>
    </section>

    <section class="section">
      <div class="section-title">Подключить Wi-Fi</div>
      <form id="wifi-form" class="card">
        <div class="field">
          <label class="field-label" for="wifi-ssid">SSID (2.4 ГГц)</label>
          <input id="wifi-ssid" class="input" type="text" maxlength="31" autocomplete="off">
        </div>
        <div class="field">
          <label class="field-label" for="wifi-pass">Пароль</label>
          <input id="wifi-pass" class="input" type="password" maxlength="63" autocomplete="off">
        </div>
        <button class="btn btn-primary btn-block" type="submit">Подключиться</button>
      </form>
      <p class="hint mt-8">
        MQTT, Telegram и статический IP настраиваются через соответствующие
        API; этот экран меняет только Wi-Fi STA.
      </p>
    </section>
  `;

  try {
    const response = await fetch('/api/health');
    const data = await response.json();
    $('#wifi-status').textContent = `${data.wifi_rssi || 0} dBm · ${data.level || '—'}`;
  } catch (_) {
    $('#wifi-status').textContent = 'Нет данных';
  }

  $('#wifi-form')?.addEventListener('submit', async (event) => {
    event.preventDefault();
    const ssid = ($('#wifi-ssid')?.value || '').trim();
    const pass = $('#wifi-pass')?.value || '';
    if (!ssid) { toast('Укажите SSID'); return; }
    try {
      const response = await postJson('/api/wifi', { ssid, pass });
      if (!response.ok) throw new Error(String(response.status));
      toast('Устройство переподключается к Wi-Fi');
    } catch (_) { toast('Не удалось изменить Wi-Fi'); }
  });
}

async function renderOta() {
  const app = $('#app');
  if (!app) return;

  app.innerHTML = `
    <section class="section">
      <div class="card">
        <div class="card-row">
          <div class="card-row-main">
            <div class="card-row-title">Текущая прошивка</div>
            <div class="card-row-sub" id="ota-status">Загрузка…</div>
          </div>
        </div>
      </div>
    </section>

    <section class="section">
      <div class="section-title">Обновление по URL</div>
      <form id="ota-form" class="card">
        <div class="field">
          <label class="field-label" for="ota-url">HTTPS-ссылка на .bin</label>
          <input id="ota-url" class="input" type="url" placeholder="https://example.com/automatika_v2.bin">
        </div>
        <button class="btn btn-primary btn-block" type="submit">Проверить и обновить</button>
        <p class="hint mt-8">
          Нужен HTTPS с валидным сертификатом. После перезагрузки прошивка
          проходит самодиагностику; при ошибке загрузчик откатится.
        </p>
      </form>
    </section>
  `;

  try {
    const response = await fetch('/api/ota');
    const data = await response.json();
    $('#ota-status').textContent = data.version ? `Версия ${data.version}` : JSON.stringify(data);
  } catch (_) {
    $('#ota-status').textContent = 'Нет данных';
  }

  $('#ota-form')?.addEventListener('submit', async (event) => {
    event.preventDefault();
    const url = ($('#ota-url')?.value || '').trim();
    if (!url) { toast('Укажите URL'); return; }
    if (!window.confirm('Запустить обновление прошивки?')) return;
    try {
      const response = await postJson('/api/ota/url', { url });
      if (!response.ok) throw new Error(String(response.status));
      toast('Обновление запущено');
    } catch (_) { toast('Не удалось запустить обновление'); }
  });
}

function renderApi() {
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

  loadFirmwareVersion();
  updateInstallHint();
}

async function renderLang() {
  const app = $('#app');
  if (!app) return;

  app.innerHTML = `
    <section class="section">
      <div class="card">
        <div class="field">
          <span class="field-label">Язык интерфейса устройства</span>
          <div class="segmented" id="lang-seg">
            <button type="button" data-lang="ru" class="active">Русский</button>
            <button type="button" data-lang="en">English</button>
          </div>
        </div>
        <p class="hint">Язык меняет экран устройства; веб-интерфейс пока русскоязычный.</p>
      </div>
    </section>
  `;

  try {
    const response = await api('/api/i18n');
    if (response.ok) {
      const data = await response.json();
      $$('#lang-seg button').forEach((button) => {
        button.classList.toggle('active', button.dataset.lang === data.lang);
      });
    }
  } catch (_) {}

  $$('#lang-seg button').forEach((button) => {
    button.addEventListener('click', async () => {
      try {
        const response = await postJson('/api/i18n', { lang: button.dataset.lang });
        if (!response.ok) throw new Error(String(response.status));
        $$('#lang-seg button').forEach((b) => b.classList.remove('active'));
        button.classList.add('active');
        toast('Язык сохранён');
      } catch (_) { toast('Не удалось изменить язык'); }
    });
  });
}

async function renderAbout() {
  const app = $('#app');
  if (!app) return;

  app.innerHTML = `
    <section class="section">
      <div class="card">
        <div class="card-row"><span class="card-row-title">Устройство</span><span class="card-row-value">Automatika</span></div>
        <div class="card-row"><span class="card-row-title">Платформа</span><span class="card-row-value">ESP32-S3</span></div>
        <div class="card-row"><span class="card-row-title">Прошивка</span><span class="card-row-value" id="about-version">—</span></div>
        <div class="card-row"><span class="card-row-title">Uptime</span><span class="card-row-value" id="about-uptime">—</span></div>
        <div class="card-row"><span class="card-row-title">Free heap</span><span class="card-row-value" id="about-heap">—</span></div>
        <div class="card-row"><span class="card-row-title">Интерфейс</span><span class="card-row-value mono">${APP_VERSION}</span></div>
      </div>
      <p class="hint mt-8">ESP-IDF 5.3.2 · LVGL 8.3 · PWA</p>
    </section>
  `;

  try {
    const response = await fetch('/api/health');
    const data = await response.json();
    $('#about-version').textContent = data.version || '—';
    $('#about-uptime').textContent = `${Math.floor(Number(data.uptime_s || 0) / 60)} мин`;
    $('#about-heap').textContent = `${Math.round(Number(data.free_heap || 0) / 1024)} КБ`;
  } catch (_) {}
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
    setActive('#reg-seg', data.regulator);
    if ($('#reg-baud')) $('#reg-baud').value = data.reg_baud || 9600;
    if ($('#reg-gpio')) $('#reg-gpio').value = Number.isFinite(Number(data.reg_gpio)) ? data.reg_gpio : -1;
    if ($('#reg-freq')) $('#reg-freq').value = data.reg_freq || data.pwm_freq_hz || 1000;
    if ($('#reg-res')) $('#reg-res').value = data.reg_resolution || data.pwm_resolution || 10;
    if ($('#reg-inv')) $('#reg-inv').checked = !!data.reg_inverted;
    if ($('#analog-range')) $('#analog-range').value = data.analog_range ?? 0;
    if ($('#analog-min')) $('#analog-min').value = data.analog_min_pct ?? 0;
    if ($('#analog-max')) $('#analog-max').value = data.analog_max_pct ?? 100;
    if ($('#ssvr-sqrt')) $('#ssvr-sqrt').checked = !!data.ssvr_power_sqrt;
    if ($('#ad-pid')) $('#ad-pid').checked = !!data.adaptive_pid;
    const regStatus = $('#reg-status');
    if (regStatus) {
      if (data.regulator === 0) {
        regStatus.textContent = 'Выход: внутренний DAC.';
      } else {
        regStatus.textContent =
          `Тип: ${data.regulator_name || '?'} · ${data.regulator_ready ? 'готов' : 'НЕ ГОТОВ'}` +
          ` · GPIO ${data.regulator_gpio ?? '—'}` +
          (data.pwm_freq_hz ? ` · ${data.pwm_freq_hz} Гц` : '') +
          (data.pwm_resolution ? ` · ${data.pwm_resolution} бит` : '') +
          ` · ${data.pwm_inverted ? 'активный низкий' : 'активный высокий'}` +
          (data.regulator_error ? ` · ${data.regulator_error}` : '');
      }
    }

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
    list.innerHTML = '<div class="empty">Укажите API-ключ: Система → API-ключ.</div>';
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

      const when = event.ts ? new Date(event.ts * 1000).toLocaleString() : '';

      if (event.ts) {
        const time = document.createElement('div');
        time.className = 'log-time';
        time.textContent = when;
        row.appendChild(time);
      }

      list.appendChild(row);

      const copyText = `${when} ${typeName} ${event.msg || ''}`.trim();
      attachSwipeAction(row, {
        label: 'Копировать',
        danger: false,
        removeOnAction: false,
        onAction: async () => {
          try {
            await navigator.clipboard.writeText(copyText);
            haptic('light');
            toast('Событие скопировано');
            return true;
          } catch (_) {
            haptic('error');
            toast('Буфер обмена недоступен');
            return false;
          }
        }
      });
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
  $('#install-btn')?.addEventListener('click', promptInstall);

  window.addEventListener('popstate', router);

  document.addEventListener('click', (event) => {
    const tabLink = event.target.closest('[data-tab]');
    if (tabLink) {
      event.preventDefault();
      showGroup(tabLink.dataset.tab);
      return;
    }

    const pageLink = event.target.closest('[data-page-link]');
    if (pageLink) {
      event.preventDefault();
      showPage(pageLink.dataset.pageLink);
      return;
    }

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
  pollSelection();
  loadParamDefs();
  setInterval(pollState, 4000);
  setInterval(pollSelection, 1000);
  setInterval(loadParamDefs, 4000);

  window.addEventListener('session-restore', () => showPage('dashboard'));

  initTabSwipe({
    getGroup: currentGroup,
    onSwitch: (group) => showGroup(group)
  });

  initEdgeBack({
    onBack: () => {
      if (currentPage.startsWith('group:')) {
        showPage('dashboard');
      } else if (history.length > 1) {
        history.back();
      } else {
        showPage('dashboard');
      }
    }
  });

  initPullToRefresh({
    enabled: () => currentPage === 'dashboard',
    onRefresh: () => pollState()
  });

  // Обновляем производные значения (объём, фракция) на открытой странице.
  setInterval(() => {
    if (document.hidden) return;
    if (currentPage === 'fraction') updateFractionsLive();
    else renderLiveValues();
  }, 1000);
}

document.addEventListener('DOMContentLoaded', init);
