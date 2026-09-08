// Настройки журнала.
//
// Всё, что касается форматирования, живёт в FmtTiny - там свой FmtConfig.h.
// Здесь только то, что нужно журналу.
//
// Значения переопределяются снаружи: через -D в platformio.ini или через
// #define до включения заголовка.
#pragma once

#include <FmtTiny.h>

// Размер буфера под одно сообщение. На AVR памяти мало, поэтому по умолчанию
// буфер вдвое короче.
#ifndef FMTLOG_MESSAGE_SIZE
#if defined(ARDUINO_ARCH_AVR)
#define FMTLOG_MESSAGE_SIZE 64
#else
#define FMTLOG_MESSAGE_SIZE 128
#endif
#endif

// Сколько источников журнала различает приложение. Уровень хранится для
// каждого отдельно, поэтому лишние источники стоят по байту.
#ifndef FMTLOG_SOURCE_COUNT
#define FMTLOG_SOURCE_COUNT 8
#endif

// Порог, ниже которого вызовы журнала выбрасываются на этапе компиляции: их
// образцы и значения в прошивку не попадают вовсе.
//
// Уровни: 0 trace, 1 debug, 2 info, 3 warn, 4 err, 5 critical, 6 system
#ifndef FMTLOG_COMPILE_LEVEL
#define FMTLOG_COMPILE_LEVEL 0
#endif

// --- журнал по сети (FmtTelnet.h) -----------------------------------------

// Сколько сообщений хранить для тех, кто подключится позже. Память занимается,
// только если FmtTelnet.h включён.
#ifndef FMTLOG_HISTORY_SIZE
#define FMTLOG_HISTORY_SIZE 2048
#endif

// Сколько соединений принимать одновременно. Каждое стоит клиента ядра плюс
// место в истории - десяток байт.
#ifndef FMTLOG_TELNET_CLIENTS
#define FMTLOG_TELNET_CLIENTS 4
#endif
