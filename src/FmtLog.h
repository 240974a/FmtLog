// FmtLog - журнал для Arduino: уровни, источники и приёмники вывода.
//
// Главный заголовок: включает журнал и форматирование - его даёт
// библиотека FmtTiny, подключённая отсюда же.
//
// Приёмники вывода подключаются отдельно:
//
//     #include <FmtColor.h>    цветной вывод в терминал
//     #include <FmtTelnet.h>   журнал по сети с историей
//     #include <FmtWeb.h>      журнал на веб-странице
//     #include <FmtSnmp.h>     уведомления монитору по SNMP
//     #include <FmtLoki.h>     строки в Loki, смотреть в Grafana
#pragma once

#include <FmtTiny.h>
#include "Log.h"
