// Automatika — touch-gesture library.
//
// Порог 50 px и 0.3 px/мс: короткое движение остаётся тапом.
// Горизонтальный свайп не мешает вертикальному скроллу: preventDefault
// вызывается только для оси X.

export class SwipeManager {
  constructor(el, opts = {}) {
    this.el = el;
    this.threshold = opts.threshold ?? 50;
    this.velocityMin = opts.velocityMin ?? 0.3;
    this.onLeft = opts.onLeft;
    this.onRight = opts.onRight;
    this.onUp = opts.onUp;
    this.onDown = opts.onDown;
    this.onProgress = opts.onProgress;
    this.onCancel = opts.onCancel;

    this._active = false;
    this._axis = null;
    this._startX = 0;
    this._startY = 0;
    this._startT = 0;

    this._bound = {
      start: this._onStart.bind(this),
      move: this._onMove.bind(this),
      end: this._onEnd.bind(this),
      cancel: this._onCancel.bind(this),
    };
    this.el.addEventListener('touchstart', this._bound.start, { passive: true });
    this.el.addEventListener('touchmove', this._bound.move, { passive: false });
    this.el.addEventListener('touchend', this._bound.end, { passive: true });
    this.el.addEventListener('touchcancel', this._bound.cancel, { passive: true });
  }

  destroy() {
    this.el.removeEventListener('touchstart', this._bound.start);
    this.el.removeEventListener('touchmove', this._bound.move);
    this.el.removeEventListener('touchend', this._bound.end);
    this.el.removeEventListener('touchcancel', this._bound.cancel);
  }

  _onStart(e) {
    if (e.touches.length !== 1) return;
    const t = e.touches[0];
    this._startX = t.clientX;
    this._startY = t.clientY;
    this._startT = performance.now();
    this._active = true;
    this._axis = null;
  }

  _onMove(e) {
    if (!this._active || e.touches.length !== 1) return;
    const t = e.touches[0];
    const dx = t.clientX - this._startX;
    const dy = t.clientY - this._startY;

    if (!this._axis && (Math.abs(dx) > 8 || Math.abs(dy) > 8)) {
      this._axis = Math.abs(dx) > Math.abs(dy) ? 'x' : 'y';
    }

    if (this._axis === 'x') {
      e.preventDefault();          // блокируем только горизонтальный скролл
      this.onProgress?.(dx, 'x');
    } else if (this._axis === 'y') {
      // Вертикаль не блокируем: pull-to-refresh и обычный скролл
      // обрабатываются отдельно.
      this.onProgress?.(dy, 'y');
    }
  }

  _onEnd(e) {
    if (!this._active) return;
    this._active = false;

    const t = e.changedTouches[0];
    const dx = t.clientX - this._startX;
    const dy = t.clientY - this._startY;
    const dt = Math.max(1, performance.now() - this._startT);
    const velocity = Math.sqrt(dx * dx + dy * dy) / dt;

    this._axis = null;

    if (velocity < this.velocityMin) {
      this.onCancel?.();
      return;
    }

    if (Math.abs(dx) > Math.abs(dy) && Math.abs(dx) > this.threshold) {
      dx < 0 ? this.onLeft?.(dx) : this.onRight?.(dx);
    } else if (Math.abs(dy) > Math.abs(dx) && Math.abs(dy) > this.threshold) {
      dy < 0 ? this.onUp?.(dy) : this.onDown?.(dy);
    } else {
      this.onCancel?.();
    }
  }

  _onCancel() {
    this._active = false;
    this._axis = null;
    this.onCancel?.();
  }
}

export function haptic(type = 'light') {
  if (!navigator.vibrate) return;
  const duration = {
    light: 10,
    medium: 20,
    heavy: 40,
    error: [20, 30, 20]
  }[type];
  if (duration) navigator.vibrate(duration);
}
