const ws = new WebSocket('ws://' + location.host + '/ws');
ws.onmessage = (e) => {
  try {
    const s = JSON.parse(e.data);
    if (window.pushChart) window.pushChart(s.cube);
  } catch(_) {}
};
ws.onclose = () => setTimeout(() => location.reload(), 5000);