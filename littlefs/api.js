// Тонкая обёртка над fetch: добавляет X-API-Key из localStorage.
// Ключ не хранится в коде и не подставляется по умолчанию — иначе он был бы
// известен всем, у кого есть прошивка (см. README, раздел безопасности).
function apiKeyGet() {
  try { return localStorage.getItem('automatika.apiKey') || ''; } catch (_) { return ''; }
}

function apiKeySet(key) {
  try {
    if (key) localStorage.setItem('automatika.apiKey', key);
    else localStorage.removeItem('automatika.apiKey');
  } catch (_) { /* приватный режим — ключ проживёт до закрытия вкладки */ }
}

function api(url, data, method) {
  const headers = { 'Content-Type': 'application/json' };
  const key = apiKeyGet();
  if (key) headers['X-API-Key'] = key;

  return fetch(url, {
    method: method || (data === undefined ? 'GET' : 'POST'),
    body: data === undefined ? undefined : JSON.stringify(data),
    headers
  });
}
