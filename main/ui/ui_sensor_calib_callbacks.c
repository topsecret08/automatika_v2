// Реэкспорт callbacks калибровки DS18B20
// Основные callbacks в ui_sensor_calib.c

#ifdef CONFIG_HMI_EXTERNAL_UI
#include "ui/ui_headless.h"
#else
#include "ui/ui.h"
#endif