function updateUI(s) {
  document.getElementById('cube').textContent = s.cube.toFixed(1) + '°C';
  document.getElementById('col').textContent  = s.col.toFixed(1) + '°C';
  document.getElementById('pwr').textContent  = s.pwr.toFixed(0) + '%';
  document.getElementById('stg').textContent  = s.stg;
}
function poll() {
  fetch('/api/state').then(r=>r.json()).then(updateUI).catch(()=>{});
  setTimeout(poll, 2000);
}
poll();