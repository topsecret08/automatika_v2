// Automatika Service Worker.
//
// Версия кэша привязана к прошивке: при обновлении FW_VERSION меняйте
// CACHE_NAME, иначе браузер продолжит отдавать старый app shell.
// API и WebSocket не кэшируются принципиально: это живые данные.
const CACHE_NAME = 'automatika-v293-1';
const STATIC_ASSETS = [
  '/', '/index.html', '/app.js', '/style.css',
  '/charts.js', '/api.js', '/ws.js', '/recipes.js', '/settings.js',
  '/offline.html', '/manifest.json',
  '/icons/icon-192.png', '/icons/icon-512.png',
  '/icons/icon-maskable-512.png', '/icons/apple-touch-icon.png'
];

// Установка: скачиваем оболочку приложения и сразу активируемся.
self.addEventListener('install', (e) => {
  e.waitUntil(
    caches.open(CACHE_NAME)
      .then((cache) => cache.addAll(STATIC_ASSETS))
      .then(() => self.skipWaiting())
  );
});

// Активация: удаляем кэши предыдущих версий прошивки.
self.addEventListener('activate', (e) => {
  e.waitUntil(
    caches.keys()
      .then((keys) => Promise.all(
        keys.filter((k) => k !== CACHE_NAME).map((k) => caches.delete(k))
      ))
      .then(() => self.clients.claim())
  );
});

self.addEventListener('fetch', (e) => {
  const url = new URL(e.request.url);

  // API и WebSocket — только сеть. При офлайне отдаём понятную ошибку,
  // чтобы интерфейс не «висел» на ожидании.
  if (url.pathname.startsWith('/api/') || url.pathname.startsWith('/ws')) {
    e.respondWith(fetch(e.request).catch(() =>
      new Response(JSON.stringify({ error: 'offline' }), {
        status: 503,
        headers: { 'Content-Type': 'application/json' }
      })
    ));
    return;
  }

  // Навигация: сначала сеть (свежий index.html), при офлайне — кэш,
  // затем страница-заглушка.
  if (e.request.mode === 'navigate') {
    e.respondWith(
      fetch(e.request)
        .then((resp) => {
          const clone = resp.clone();
          caches.open(CACHE_NAME).then((c) => c.put('/', clone));
          return resp;
        })
        .catch(() => caches.match('/index.html').then((r) => r || caches.match('/offline.html')))
    );
    return;
  }

  // Статика: cache-first + фоновое обновление.
  e.respondWith(
    caches.match(e.request).then((cached) => {
      const network = fetch(e.request).then((resp) => {
        if (resp.ok && e.request.method === 'GET') {
          const clone = resp.clone();
          caches.open(CACHE_NAME).then((c) => c.put(e.request, clone));
        }
        return resp;
      }).catch(() => cached || caches.match('/offline.html'));
      return cached || network;
    })
  );
});

// Push-уведомления (например, от Telegram-моста или внешнего сервиса).
self.addEventListener('push', (e) => {
  let data = { title: 'Automatika', body: 'Событие' };
  try { if (e.data) data = e.data.json(); } catch (_) {
    if (e.data) data.body = e.data.text();
  }
  e.waitUntil(self.registration.showNotification(data.title || 'Automatika', {
    body: data.body || 'Событие',
    icon: '/icons/icon-192.png',
    badge: '/icons/icon-192.png',
    vibrate: [200, 100, 200],
    tag: data.tag || 'automatika',
    requireInteraction: data.critical === true
  }));
});

// Клик по уведомлению открывает (или фокусирует) приложение.
self.addEventListener('notificationclick', (e) => {
  e.notification.close();
  e.waitUntil(
    self.clients.matchAll({ type: 'window', includeUncontrolled: true }).then((list) => {
      for (const c of list) if ('focus' in c) return c.focus();
      return self.clients.openWindow('/');
    })
  );
});
