function updateUI(s) {
  const cube = document.getElementById('cube');
  const col  = document.getElementById('col');
  const pwr  = document.getElementById('pwr');
  const stg  = document.getElementById('stg');
  if (!cube || !col || !pwr || !stg) return;

  const power = (s.pwr !== undefined) ? s.pwr : s.power;
  const stage = (s.stg !== undefined) ? s.stg : s.stage;

  if (typeof s.cube === 'number') cube.textContent = s.cube.toFixed(1) + '°C';
  if (typeof s.col  === 'number') col.textContent  = s.col.toFixed(1) + '°C';
  if (typeof power  === 'number') pwr.textContent  = power.toFixed(0) + '%';
  if (stage !== undefined && stage !== null) stg.textContent = stage;
}

function poll() {
  fetch('/api/state').then(r => r.json()).then(updateUI).catch(() => {});
  setTimeout(poll, 2000);
}
poll();
