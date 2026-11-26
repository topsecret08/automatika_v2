// Реэкспорт callbacks из modes/beer_callbacks.c
// (для сборки в CMakeLists)

#ifdef CONFIG_HMI_EXTERNAL_UI
#include "ui/ui_headless.h"
#else
#include "ui/ui.h"
#endif

// Пустой файл — callbacks живут в modes/beer_callbacks.c
// Оставлено для совместимости с CMakeLists.txt