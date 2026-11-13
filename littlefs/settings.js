function saveSetting(key, val) {
  api('/api/settings', { key, value: val });
}