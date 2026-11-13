// Плавающий индикатор активной сессии.
// Виден на всех страницах, кроме главной; тап возвращает к процессу.
import { haptic } from './gestures.js';

const API_KEY_STORAGE = 'automatika.apiKey';

const session = {
  running: false,
  visible: true,
  stage: '—',
  tSteam: 0,
  countdown: 0
};

let lastVisibility = null;
let widget = null;

function apiKey() {
  try { return localStorage.getItem(API_KEY_STORAGE) || ''; } catch (_) { return ''; }
}

function notifyBackend(path) {
  const key = apiKey();
  fetch(path, {
    method: 'POST',
    headers: key ? { 'X-API-Key': key } : {}
  }).catch(() => {});
}

function ensureWidget() {
  if (widget) return widget;
  widget = document.getElementById('session-widget');
  if (widget) return widget;

  widget = document.createElement('div');
  widget.id = 'session-widget';
  widget.setAttribute('role', 'button');
  widget.setAttribute('aria-label', 'Вернуться к процессу');
  widget.addEventListener('click', () => {
    haptic('light');
    session.visible = true;
    lastVisibility = true;
    notifyBackend('/api/session/restore');
    renderWidget();
    window.dispatchEvent(new CustomEvent('session-restore'));
  });
  document.body.appendChild(widget);
  return widget;
}

export function updateSession(data = {}) {
  if (typeof data.running === 'boolean') session.running = data.running;
  if (typeof data.visible === 'boolean') session.visible = data.visible;
  if (data.stage) session.stage = data.stage;
  if ('tSteam' in data) session.tSteam = Number(data.tSteam);
  if ('countdown' in data) session.countdown = Number(data.countdown);
  renderWidget();
}

export function updateMinimizeState(page, running) {
  const visible = page === 'dashboard';
  const wasRunning = session.running;

  session.running = !!running;
  session.visible = visible;

  if (session.running && lastVisibility !== visible) {
    lastVisibility = visible;
    notifyBackend(visible ? '/api/session/restore' : '/api/session/minimize');
  }
  if (!session.running) lastVisibility = null;
  if (wasRunning !== session.running) haptic('light');

  renderWidget();
}

function renderWidget() {
  const node = ensureWidget();
  if (!session.running || session.visible) {
    node.style.display = 'none';
    return;
  }

  const tempText = Number.isFinite(session.tSteam)
    ? session.tSteam.toFixed(1) + '°'
    : '--.-°';
  const countdownText = Number.isFinite(session.countdown)
    ? session.countdown.toFixed(1) + ' с'
    : '—';

  node.style.display = 'flex';
  node.innerHTML = `
    <span class="sw-dot"></span>
    <div class="sw-info">
      <div class="sw-title">Работа · ${session.stage}</div>
      <div class="sw-sub">${tempText} · до смены ${countdownText}</div>
    </div>
    <span class="sw-return">→</span>
  `;
}
