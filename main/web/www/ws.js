let ws = null;
let wsTimer = null;

function updateFromWS(s) {
  if (typeof updateUI === 'function') updateUI(s);
  if (window.pushChart && typeof s.cube === 'number') window.pushChart(s.cube);
}

function connectWS() {
  const proto = location.protocol === 'https:' ? 'wss://' : 'ws://';
  ws = new WebSocket(proto + location.host + '/ws');

  ws.onopen = () => {
    // Сервер отвечает состоянием на каждый запрос.
    wsTimer = setInterval(() => {
      if (ws && ws.readyState === WebSocket.OPEN) ws.send('get');
    }, 1000);
  };

  ws.onmessage = (e) => {
    try { updateFromWS(JSON.parse(e.data)); } catch (_) {}
  };

  ws.onerror = () => { try { ws.close(); } catch (_) {} };

  ws.onclose = () => {
    if (wsTimer) { clearInterval(wsTimer); wsTimer = null; }
    setTimeout(connectWS, 5000);
  };
}

connectWS();
