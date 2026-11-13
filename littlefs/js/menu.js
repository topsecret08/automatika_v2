// Automatika — единая карта меню для PWA.
//
// Группировка по задачам пользователя, а не по функциям (iOS HIG):
// Работа, Подготовка, Диагностика, Система. Внутри — не более 4 пунктов.
// Этот файл не знает, как рендерить страницы: только данные и разметка
// навигации. app.js импортирует MENU и рендерит конкретную page.

export const MENU = {
  work: {
    label: 'Работа',
    icon: 'play',
    tasks: [
      { id: 'dashboard', label: 'Дашборд', page: 'dashboard', icon: 'grid' },
      { id: 'fraction', label: 'Текущая фракция', page: 'fraction', icon: 'layers' },
    ],
  },
  prepare: {
    label: 'Подготовка',
    icon: 'sliders',
    tasks: [
      { id: 'profiles', label: 'Профили', page: 'profiles', icon: 'folder' },
      { id: 'calibration', label: 'Калибровка насоса', page: 'calibration', icon: 'gauge' },
      { id: 'autotune', label: 'PID-автотюн', page: 'autotune', icon: 'sliders' },
      { id: 'sensors', label: 'Проверка датчиков', page: 'sensors', icon: 'thermometer' },
      { id: 'selection', label: 'Вариант отбора', page: 'selection', icon: 'layers' },
      { id: 'ssvc', label: 'Параметры SSVC', page: 'ssvc', icon: 'gauge' },
    ],
  },
  diagnose: {
    label: 'Диагностика',
    icon: 'activity',
    tasks: [
      { id: 'log', label: 'Журнал событий', page: 'log', icon: 'list' },
      { id: 'actuators', label: 'Проверка приводов', page: 'actuators', icon: 'tool' },
      { id: 'health', label: 'Состояние', page: 'health', icon: 'heart' },
      { id: 'network', label: 'Сеть', page: 'network', icon: 'wifi' },
    ],
  },
  system: {
    label: 'Система',
    icon: 'gear',
    tasks: [
      { id: 'ota', label: 'Обновление', page: 'ota', icon: 'download' },
      { id: 'api', label: 'API-ключ', page: 'api', icon: 'key' },
      { id: 'lang', label: 'Язык', page: 'lang', icon: 'globe' },
      { id: 'about', label: 'О устройстве', page: 'about', icon: 'info' },
    ],
  },
};

const ICONS = {
  play: '<path d="M8 5v14l11-7z"/>',
  sliders: '<path d="M4 21v-7M4 10V3M12 21v-9M12 8V3M20 21v-5M20 12V3"/><path d="M1 14h6M9 8h6M17 16h6"/>',
  activity: '<path d="M3 12h4l3-8 4 16 3-8h4"/>',
  gear: '<circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.7 1.7 0 0 0 .3 1.9l.1.1a2 2 0 1 1-2.8 2.8l-.1-.1a1.7 1.7 0 0 0-1.9-.3 1.7 1.7 0 0 0-1 1.5V21a2 2 0 1 1-4 0v-.1a1.7 1.7 0 0 0-1.1-1.5 1.7 1.7 0 0 0-1.9.3l-.1.1a2 2 0 1 1-2.8-2.8l.1-.1a1.7 1.7 0 0 0 .3-1.9 1.7 1.7 0 0 0-1.5-1H3a2 2 0 1 1 0-4h.1a1.7 1.7 0 0 0 1.5-1.1 1.7 1.7 0 0 0-.3-1.9l-.1-.1a2 2 0 1 1 2.8-2.8l.1.1a1.7 1.7 0 0 0 1.9.3H9a1.7 1.7 0 0 0 1-1.5V3a2 2 0 1 1 4 0v.1a1.7 1.7 0 0 0 1 1.5 1.7 1.7 0 0 0 1.9-.3l.1-.1a2 2 0 1 1 2.8 2.8l-.1.1a1.7 1.7 0 0 0-.3 1.9V9a1.7 1.7 0 0 0 1.5 1H21a2 2 0 1 1 0 4h-.1a1.7 1.7 0 0 0-1.5 1z"/>',
  grid: '<rect x="3" y="3" width="7" height="7" rx="1.5"/><rect x="14" y="3" width="7" height="7" rx="1.5"/><rect x="3" y="14" width="7" height="7" rx="1.5"/><rect x="14" y="14" width="7" height="7" rx="1.5"/>',
  layers: '<path d="M12 2 2 7l10 5 10-5-10-5z"/><path d="m2 12 10 5 10-5"/><path d="m2 17 10 5 10-5"/>',
  folder: '<path d="M3 7a2 2 0 0 1 2-2h5l2 2h7a2 2 0 0 1 2 2v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/>',
  gauge: '<path d="M12 14a2 2 0 1 0 0-4 2 2 0 0 0 0 4z"/><path d="M13.4 10.6 19 5"/><path d="M4 18a8 8 0 1 1 16 0"/>',
  thermometer: '<path d="M14 14.8V5a2 2 0 1 0-4 0v9.8a4 4 0 1 0 4 0z"/>',
  list: '<path d="M8 6h13M8 12h13M8 18h13"/><path d="M3 6h.01M3 12h.01M3 18h.01"/>',
  tool: '<path d="M14.7 6.3a4 4 0 0 0-5.4 5.4L3 18l3 3 6.3-6.3a4 4 0 0 0 5.4-5.4l-2.3 2.3-2.4-.5-.5-2.4z"/>',
  heart: '<path d="M20.8 4.6a5.5 5.5 0 0 0-7.8 0L12 5.7l-1-1.1a5.5 5.5 0 0 0-7.8 7.8l1 1L12 21l7.8-7.6 1-1a5.5 5.5 0 0 0 0-7.8z"/>',
  wifi: '<path d="M5 12.5a10 10 0 0 1 14 0"/><path d="M8.5 16a5 5 0 0 1 7 0"/><path d="M12 20h.01"/>',
  download: '<path d="M12 3v12"/><path d="m7 10 5 5 5-5"/><path d="M5 21h14"/>',
  key: '<circle cx="7.5" cy="15.5" r="4.5"/><path d="m11 12 9-9"/><path d="m17 4 3 3"/><path d="m15 6 2 2"/>',
  globe: '<circle cx="12" cy="12" r="9"/><path d="M3 12h18"/><path d="M12 3a15 15 0 0 1 0 18a15 15 0 0 1 0-18z"/>',
  info: '<circle cx="12" cy="12" r="9"/><path d="M12 11v5"/><path d="M12 8h.01"/>',
};

export function icon(name) {
  const body = ICONS[name] || ICONS.play;
  return `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">${body}</svg>`;
}

export function groupForPage(page) {
  for (const [key, group] of Object.entries(MENU)) {
    if (group.tasks.some((task) => task.page === page)) return key;
  }
  return 'work';
}

export function taskByPage(page) {
  for (const group of Object.values(MENU)) {
    const task = group.tasks.find((item) => item.page === page);
    if (task) return task;
  }
  return null;
}

export function pageTitle(page) {
  const task = taskByPage(page);
  if (task) return task.label;
  return 'Automatika';
}

export function renderTabBar(activeGroup) {
  const bar = document.querySelector('.tab-bar');
  if (!bar) return;

  bar.innerHTML = Object.entries(MENU).map(([key, group]) => `
    <a href="/?tab=${key}" data-tab="${key}"
       class="${key === activeGroup ? 'active' : ''}">
      ${icon(group.icon)}
      <span>${group.label}</span>
    </a>
  `).join('');
}

export function renderSubMenu(groupKey) {
  const group = MENU[groupKey] || MENU.work;
  const rows = group.tasks.map((task) => `
    <a class="task-row" href="/?page=${task.page}" data-page-link="${task.page}">
      <span class="task-icon">${icon(task.icon)}</span>
      <span class="body grow">${task.label}</span>
      <span class="chevron" aria-hidden="true">›</span>
    </a>
  `).join('');

  return `
    <section class="section">
      <div class="section-title">${group.label}</div>
      <div class="card task-list">${rows}</div>
    </section>
    <p class="hint">
      Группы соответствуют задачам: «Работа» — ежедневное управление,
      «Подготовка» — перед запуском, «Диагностика» — при проблемах,
      «Система» — редкие настройки.
    </p>
  `;
}
