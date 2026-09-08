// FmtLog - форматирование по образцу и журнал для Arduino.
//
// Главный заголовок: включает форматирование и журнал.
//
// Отдельно подключаются:
// Форматирование даёт библиотека FmtTiny, включённая отсюда же.
//
//     #include <FmtColor.h>    цветной вывод в терминал
//     #include <FmtTelnet.h>   журнал по сети с историей
//     #include <FmtEeprom.h>   вывод строк из EEPROM
#pragma once

#include <FmtTiny.h>
#include "Log.h"
