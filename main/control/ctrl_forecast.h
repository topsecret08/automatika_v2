#pragma once

void  ctrl_forecast_update(float temp, float dt);
float ctrl_forecast_rate(void);             // °C/мин
float ctrl_forecast_predict(float sec);     // прогноз через N сек
float ctrl_forecast_time_to(float target);  // сек до цели