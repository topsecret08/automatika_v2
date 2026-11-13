// WebSocket-телеметрия PWA.
//
// Сервер отдаёт кадр раз в секунду на /ws/telemetry:
//   {tc, ts, p, m, sa, es, lv, r, stg}
// Если сокет закрыт, app.js продолжает опрашивать /api/state (5 с), поэтому
// интерфейс остаётся работоспособным и без WebSocket.

let ws = null;
let wsRetry = null;

function wsUrl() {
  const proto = location.protocol === 'https:' ? 'wss://' : 'ws://';
  return proto + location.host + '/ws/telemetry';
}

function wsConnect() {
  if (ws && (ws.readyState === WebSocket.OPEN || ws.readyState === WebSocket.CONNECTING)) return;

  try {
    ws = new WebSocket(wsUrl());
  } catch (_) {
    wsScheduleRetry();
    return;
  }

  ws.onopen = () => {
    if (wsRetry) { clearTimeout(wsRetry); wsRetry = null; }
    if (typeof setStatus === 'function') setStatus(true);
  };

  ws.onmessage = (e) => {
    try {
      const data = JSON.parse(e.data);
      if (typeof updateUI === 'function') updateUI(data);
    } catch (_) { /* некорректный кадр игнорируем */ }
  };

  ws.onerror = () => { try { ws.close(); } catch (_) {} };

  ws.onclose = () => {
    if (typeof setStatus === 'function') setStatus(false);
    wsScheduleRetry();
  };
}

function wsScheduleRetry() {
  if (wsRetry) return;
  wsRetry = setTimeout(() => {
    wsRetry = null;
    wsConnect();
  }, 3000);
}

// При возврате вкладки из фона браузер мог закрыть сокет — переподключаемся.
document.addEventListener('visibilitychange', () => {
  if (!document.hidden) wsConnect();
});
window.addEventListener('online', wsConnect);

wsConnect();
