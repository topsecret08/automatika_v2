// Pull-to-refresh только на дашборде.

import { haptic } from './gestures.js';

export function initPullToRefresh({ enabled, onRefresh } = {}) {
  const app = document.getElementById('app');
  if (!app) return null;

  const indicator = document.createElement('div');
  indicator.className = 'ptr-indicator';
  const spinner = document.createElement('div');
  spinner.className = 'ptr-spinner';
  indicator.appendChild(spinner);
  document.body.appendChild(indicator);

  let startY = 0;
  let pulling = false;
  let distance = 0;
  const MAX = 100;

  const hide = () => {
    indicator.style.transform = 'translate(-50%, -60px) scale(0.5)';
    indicator.style.opacity = '0';
  };

  app.addEventListener('touchstart', (event) => {
    if (enabled && !enabled()) return;
    if (window.scrollY > 0) return;
    startY = event.touches[0].clientY;
    pulling = true;
    distance = 0;
  }, { passive: true });

  app.addEventListener('touchmove', (event) => {
    if (!pulling) return;
    distance = Math.max(0, Math.min(event.touches[0].clientY - startY, MAX));
    if (distance <= 0) return;

    event.preventDefault();
    const scale = 0.5 + (distance / MAX) * 0.5;
    indicator.style.transform = `translate(-50%, ${distance - 60}px) scale(${scale})`;
    indicator.style.opacity = String(distance / MAX);
  }, { passive: false });

  app.addEventListener('touchend', () => {
    if (!pulling) return;
    pulling = false;

    if (distance > 70) {
      haptic('medium');
      indicator.classList.add('refreshing');
      indicator.style.transform = 'translate(-50%, 0) scale(1)';
      indicator.style.opacity = '1';

      Promise.resolve(onRefresh ? onRefresh() : null).finally(() => {
        indicator.classList.remove('refreshing');
        hide();
      });
    } else {
      hide();
    }
    distance = 0;
  });

  return { hide };
}
