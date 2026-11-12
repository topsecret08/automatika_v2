#pragma once
#include "esp_http_server.h"

// Диагностика исполнительных механизмов (ТОЛЬКО в сервисном режиме):
//
//   POST /api/diag/actuators   {"actuator":"valve","action":"open"}
//                              {"actuator":"valve","action":"close"}
//                              {"actuator":"relay","ch":0,"state":1}
//                              {"actuator":"pump","action":"dose","ml":10,"rate":60}
//                              {"actuator":"heater","action":"on","pct":10,"seconds":5}
//                              {"actuator":"heater","action":"off"}
//   POST /api/diag/service     {"enabled":true}   (только с API-ключом)
//
// Требуется X-API-Key (api_check_auth) и сервисный режим
// (control_config_service_enabled). Нагрев и дозатор проходят через
// инварианты безопасности (ctrl_safety_ok, E-STOP); heater включается
// «service»-путём с автовыключением.

void actuator_test_register(httpd_handle_t srv);
