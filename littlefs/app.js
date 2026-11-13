// Автоматика: логика PWA-оболочки.
//
// Здесь нет бизнес-логики устройства: только отображение телеметрии,
// маршрутизация вкладок, установка PWA и хранение API-ключа.
// Живые данные приходят по WebSocket (ws.js) на /ws/telemetry; если сокет
// недоступен, включается редкий опрос /api/state.

const APP_VERSION = '2.9.3-pwa.1';
const $ = (id) => document.getElementById(id);

// ------------------------------------------------------------------
// Отображение телеметрии
// ------------------------------------------------------------------
function updateUI(s) {
  if (!s || typeof s !== 'object') return;

  // /api/state отдаёт cube/col/power/stage, /ws/telemetry — tc/ts/p/stg.
  const cube  = (typeof s.cube  === 'number') ? s.cube  : s.tc;
  const col   = (typeof s.col   === 'number') ? s.col   : s.ts;
  const power = (typeof s.power === 'number') ? s.power : s.p;
  const stage = (s.stage !== undefined) ? s.stage : s.stg;

  if ($('cube') && typeof cube === 'number')  $('cube').textContent = cube.toFixed(1) + ' °C';
  if ($('col')  && typeof col  === 'number')  $('col').textContent  = col.toFixed(1) + ' °C';
  if ($('pwr')  && typeof power === 'number') $('pwr').textContent  = power.toFixed(0) + '%';
  if ($('stg')  && stage !== undefined && stage !== null) $('stg').textContent = stage;

  if (window.pushChart && typeof cube === 'number') window.pushChart(cube);
}

function setStatus(online) {
  const el = $('connection-status');
  if (!el) return;
  el.textContent = online ? 'Online' : 'Offline';
  el.className = 'status ' + (online ? 'online' : 'offline');
}

// ------------------------------------------------------------------
// Действия
// ------------------------------------------------------------------
async function runAction(url) {
  const out = $('action-result');
  if (!apiKeyGet()) {
    if (out) out.textContent = 'Нужен API-ключ: вкладка «Настройки».';
    location.hash = '#settings';
    return;
  }
  try {
    const r = await api(url, {});
    if (out) out.textContent = r.ok ? 'Команда принята' : ('Ошибка ' + r.status);
  } catch (_) {
    if (out) out.textContent = 'Нет связи с устройством';
  }
}

// ------------------------------------------------------------------
// Журнал событий
// ------------------------------------------------------------------
async function loadLog() {
  const list = $('log-list');
  if (!list) return;
  list.textContent = '';

  if (!apiKeyGet()) {
    list.textContent = 'Укажите API-ключ на вкладке «Настройки».';
    return;
  }

  try {
    const r = await api('/api/events?limit=50');
    if (!r.ok) { list.textContent = 'Ошибка ' + r.status; return; }
    const events = await r.json();
    if (!Array.isArray(events) || events.length === 0) {
      list.textContent = 'Журнал пуст.';
      return;
    }
    // textContent, а не innerHTML: сообщения событий не должны исполняться.
    for (const ev of events) {
      const row = document.createElement('div');
      row.className = 'log-row';
      const when = ev.ts ? new Date(ev.ts * 1000).toLocaleString() : '';
      row.textContent = `${when}  ${ev.type || ''}  ${ev.msg || ''}`;
      list.appendChild(row);
    }
  } catch (_) {
    list.textContent = 'Нет связи с устройством.';
  }
}

// ------------------------------------------------------------------
// Навигация по hash (PWA shortcuts: /#dashboard, /#log, /#settings)
// ------------------------------------------------------------------
function route() {
  const page = (location.hash || '#dashboard').replace('#', '');
  const known = ['dashboard', 'log', 'settings'];
  const active = known.includes(page) ? page : 'dashboard';

  for (const name of known) {
    const el = $('page-' + name);
    if (el) el.hidden = (name !== active);
    const link = document.querySelector(`#bottom-nav a[data-page="${name}"]`);
    if (link) link.classList.toggle('active', name === active);
  }

  if (active === 'log') loadLog();
}

// ------------------------------------------------------------------
// Установка PWA (Add to Home Screen)
// ------------------------------------------------------------------
let deferredPrompt = null;

window.addEventListener('beforeinstallprompt', (e) => {
  e.preventDefault();
  deferredPrompt = e;
  const btn = $('install-btn');
  if (btn) btn.hidden = false;
});

window.addEventListener('appinstalled', () => {
  deferredPrompt = null;
  const btn = $('install-btn');
  if (btn) btn.hidden = true;
});

// ------------------------------------------------------------------
// Инициализация
// ------------------------------------------------------------------
function init() {
  const power = $('power');
  if (power) {
    power.addEventListener('input', () => {
      const p = $('pwrset');
      if (p) p.textContent = power.value + '%';
    });
    power.addEventListener('change', () => {
      if (!apiKeyGet()) {
        const out = $('action-result');
        if (out) out.textContent = 'Нужен API-ключ: вкладка «Настройки».';
        return;
      }
      api('/api/power', { power: Number(power.value) }).catch(() => {});
    });
  }

  document.querySelectorAll('[data-action]').forEach((btn) => {
    btn.addEventListener('click', () => runAction(btn.dataset.action));
  });

  const install = $('install-btn');
  if (install) {
    install.addEventListener('click', async () => {
      if (!deferredPrompt) return;
      deferredPrompt.prompt();
      try { await deferredPrompt.userChoice; } catch (_) {}
      deferredPrompt = null;
      install.hidden = true;
    });
  }

  const keyInput = $('api-key');
  if (keyInput) keyInput.value = apiKeyGet();

  const save = $('key-save');
  if (save) save.addEventListener('click', () => {
    apiKeySet((keyInput && keyInput.value.trim()) || '');
    route();
  });

  const clear = $('key-clear');
  if (clear) clear.addEventListener('click', () => {
    apiKeySet('');
    if (keyInput) keyInput.value = '';
    const st = $('key-status');
    if (st) st.textContent = 'Ключ удалён из этого браузера.';
  });

  // Первичная выдача ключа. Устройство разрешает этот запрос без ключа
  // только пока ключ ещё не создан (bootstrap). Если ключ уже есть,
  // api.js добавит текущий X-API-Key и произойдёт ротация.
  const gen = $('key-generate');
  if (gen) gen.addEventListener('click', async () => {
    const st = $('key-status');
    if (st) st.textContent = 'Запрашиваю ключ…';
    try {
      const r = await api('/api/config/generate-key', undefined, 'POST');
      if (!r.ok) {
        if (st) st.textContent = 'Ошибка ' + r.status +
          (r.status === 401 ? ': сначала укажите текущий ключ.' : '.');
        return;
      }
      const data = await r.json();
      if (data && data.api_key) {
        apiKeySet(data.api_key);
        if (keyInput) keyInput.value = data.api_key;
        if (st) st.textContent = 'Ключ создан и сохранён в этом браузере.';
      } else {
        if (st) st.textContent = 'Устройство не вернуло ключ.';
      }
    } catch (_) {
      if (st) st.textContent = 'Нет связи с устройством.';
    }
  });

  const refresh = $('log-refresh');
  if (refresh) refresh.addEventListener('click', loadLog);

  const version = $('app-version');
  if (version) version.textContent = APP_VERSION;

  window.addEventListener('hashchange', route);
  route();

  // Резервный опрос, пока WebSocket не подключён (ws.js обновляет статус).
  setInterval(() => {
    if (document.hidden) return;
    const st = $('connection-status');
    if (st && st.classList.contains('online')) return;
    fetch('/api/state').then((r) => r.json())
      .then((s) => { updateUI(s); setStatus(true); })
      .catch(() => setStatus(false));
  }, 5000);

  // Регистрация Service Worker. Требуется secure context:
  // https://<IP>/ с доверенным сертификатом либо http://localhost.
  if ('serviceWorker' in navigator) {
    window.addEventListener('load', () => {
      navigator.serviceWorker.register('/sw.js')
        .catch((err) => console.warn('SW registration failed', err));
    });
  }
}

document.addEventListener('DOMContentLoaded', init);
