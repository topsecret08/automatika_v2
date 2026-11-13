// Свайп по строке списка: слева открывается действие.
//
// В журнале события нельзя удалить по одному (в прошивке нет такого API),
// поэтому там используется действие «Копировать». В профилях — «Удалить».

import { SwipeManager, haptic } from './gestures.js';

const MAX = -84;

export function attachSwipeAction(el, options = {}) {
  if (!el || !el.parentNode) return null;

  const label = options.label || 'Удалить';
  const danger = options.danger !== false;
  const onAction = options.onAction;

  const wrapper = document.createElement('div');
  wrapper.className = 'swipe-row';
  el.parentNode.insertBefore(wrapper, el);

  const actions = document.createElement('div');
  actions.className = 'swipe-actions';

  const button = document.createElement('button');
  button.type = 'button';
  button.className = 'btn-delete' + (danger ? '' : ' copy');
  button.textContent = label;
  actions.appendChild(button);

  wrapper.appendChild(actions);
  wrapper.appendChild(el);
  el.classList.add('swipe-content');

  let offset = 0;

  const settle = (value) => {
    offset = value;
    el.style.transition = 'transform .25s cubic-bezier(0.25, 0.1, 0.25, 1)';
    el.style.transform = offset === 0 ? '' : `translateX(${offset}px)`;
  };

  const manager = new SwipeManager(el, {
    threshold: 30,
    velocityMin: 0.25,
    onProgress(dx) {
      if (dx > 0) return;
      offset = Math.max(dx, MAX);
      el.style.transition = 'none';
      el.style.transform = `translateX(${offset}px)`;
    },
    onLeft() {
      settle(MAX);
      haptic('light');
    },
    onRight() {
      settle(0);
    },
    onCancel() {
      settle(offset < MAX / 2 ? 0 : MAX);
    },
  });

  button.addEventListener('click', async (event) => {
    event.stopPropagation();
    haptic('heavy');
    let ok = true;
    try {
      if (onAction) ok = await onAction();
    } catch (_) {
      ok = false;
    }
    if (ok === false) return;

    if (options.removeOnAction === false) {
      settle(0);
      return;
    }

    el.classList.add('removing');
    setTimeout(() => {
      manager.destroy();
      wrapper.remove();
    }, 250);
  });

  return {
    close: () => settle(0),
    destroy: () => {
      manager.destroy();
      wrapper.remove();
    },
  };
}

export const attachSwipeDelete = attachSwipeAction;
