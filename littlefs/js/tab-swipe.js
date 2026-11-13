// Свайп между четырьмя группами меню + edge-swipe «Назад».

import { SwipeManager, haptic } from './gestures.js';

const TAB_ORDER = ['work', 'prepare', 'diagnose', 'system'];
const EASE = 'cubic-bezier(0.25, 0.1, 0.25, 1)';

function resetTransform(app) {
  if (!app) return;
  app.style.transition = `transform .28s ${EASE}, opacity .28s ${EASE}`;
  app.style.transform = '';
  app.style.opacity = '1';
}

export function initTabSwipe({ getGroup, onSwitch }) {
  const app = document.getElementById('app');
  if (!app) return null;

  let switching = false;

  const switchTab = (nextTab, direction) => {
    if (switching) return;
    switching = true;

    app.style.transition = `transform .2s ${EASE}, opacity .2s ${EASE}`;
    app.style.transform = `translateX(${direction === 'left' ? '-100%' : '100%'})`;
    app.style.opacity = '0';

    setTimeout(() => {
      onSwitch(nextTab);
      app.style.transition = 'none';
      app.style.transform = `translateX(${direction === 'left' ? '100%' : '-100%'})`;
      requestAnimationFrame(() => {
        app.style.transition = `transform .28s ${EASE}, opacity .28s ${EASE}`;
        app.style.transform = '';
        app.style.opacity = '1';
        switching = false;
      });
    }, 180);
  };

  return new SwipeManager(app, {
    threshold: 50,
    velocityMin: 0.3,

    onProgress(dx, axis) {
      if (axis !== 'x' || switching) return;
      app.style.transition = 'none';
      app.style.transform = `translateX(${dx * 0.35}px)`;
      app.style.opacity = String(1 - Math.min(Math.abs(dx) / 400, 0.4));
    },

    onLeft() {
      const index = TAB_ORDER.indexOf(getGroup());
      haptic('light');
      if (index >= 0 && index < TAB_ORDER.length - 1) {
        switchTab(TAB_ORDER[index + 1], 'left');
      } else {
        resetTransform(app);
      }
    },

    onRight() {
      const index = TAB_ORDER.indexOf(getGroup());
      haptic('light');
      if (index > 0) {
        switchTab(TAB_ORDER[index - 1], 'right');
      } else {
        resetTransform(app);
      }
    },

    onCancel() {
      resetTransform(app);
    },
  });
}

export function initEdgeBack({ onBack }) {
  const edge = document.createElement('div');
  edge.className = 'edge-back';
  edge.setAttribute('aria-hidden', 'true');
  document.body.appendChild(edge);

  return new SwipeManager(edge, {
    threshold: 40,
    velocityMin: 0.3,
    onRight() {
      haptic('light');
      onBack();
    },
  });
}
