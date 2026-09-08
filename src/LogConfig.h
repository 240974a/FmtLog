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

// --- журнал на веб-страницу (FmtWeb.h) ------------------------------------

// Сколько строк хранить для вкладок, открытых позже. Память занимается,
// только если FmtWeb.h включён.
#ifndef FMTLOG_WEB_HISTORY_SIZE
#define FMTLOG_WEB_HISTORY_SIZE 4096
#endif

// Сколько вкладок обслуживать одновременно.
#ifndef FMTLOG_WEB_CLIENTS
#define FMTLOG_WEB_CLIENTS 2
#endif

// --- уведомления по SNMP (FmtSnmp.h) --------------------------------------

// Сколько trap'ов держать, пока их не отправит handle(). Они редки, а
// если сеть лежит дольше, чем вмещает очередь, дежурному важнее свежая -
// самая старая теряется, и это видно в snmp::lost().
#ifndef FMTLOG_SNMP_QUEUE
#define FMTLOG_SNMP_QUEUE 4
#endif

// Сколько знаков сообщения уходит в trap. Длинный текст читают в журнале,
// а здесь важна суть: что случилось и где.
#ifndef FMTLOG_SNMP_TEXT_SIZE
#define FMTLOG_SNMP_TEXT_SIZE 96
#endif

// Буфер под собранный пакет. Больше сообщения с запасом на обёртки BER.
#ifndef FMTLOG_SNMP_PACKET_SIZE
#define FMTLOG_SNMP_PACKET_SIZE 256
#endif

// Длина community-строки и корня OID вместе с завершающим нулём.
#ifndef FMTLOG_SNMP_COMMUNITY_SIZE
#define FMTLOG_SNMP_COMMUNITY_SIZE 24
#endif
#ifndef FMTLOG_SNMP_OID_SIZE
#define FMTLOG_SNMP_OID_SIZE 48
#endif

// Из скольких чисел может состоять OID. Стандартные ветки короче десятка.
#ifndef FMTLOG_SNMP_OID_PARTS
#define FMTLOG_SNMP_OID_PARTS 16
#endif

// --- журнал в Loki (FmtLoki.h) --------------------------------------------

// Сколько строк копится до отправки. Пачкой дешевле: у Loki на каждый запрос
// свои накладные, и сетевые, и на его стороне.
#ifndef FMTLOG_LOKI_BATCH
#define FMTLOG_LOKI_BATCH 16
#endif

// Через сколько миллисекунд отправлять неполную пачку. Иначе редкие строки
// ждали бы в буфере, пока не наберётся полная.
#ifndef FMTLOG_LOKI_INTERVAL_MS
#define FMTLOG_LOKI_INTERVAL_MS 5000
#endif

// Сколько знаков строки уходит в Loki.
#ifndef FMTLOG_LOKI_TEXT_SIZE
#define FMTLOG_LOKI_TEXT_SIZE 96
#endif

// Сколько своих меток задаёт приложение. Меток должно быть немного: в Loki
// каждое их сочетание заводит отдельный поток.
#ifndef FMTLOG_LOKI_LABELS
#define FMTLOG_LOKI_LABELS 4
#endif

// Длина имени и значения метки вместе с завершающим нулём.
#ifndef FMTLOG_LOKI_LABEL_SIZE
#define FMTLOG_LOKI_LABEL_SIZE 24
#endif

// Буфер под тело запроса. Он должен вмещать всю пачку целиком: строка
// занимает свою длину плюс около тридцати знаков на отметку времени и
// скобки, а экранирование кавычек может её и удлинить.
//
// По умолчанию берётся с запасом от FMTLOG_LOKI_BATCH и FMTLOG_LOKI_TEXT_SIZE
// - меняя их, про буфер можно не вспоминать. Пачка, не влезшая в буфер, не
// уйдёт вовсе, и это будет видно в loki::lost().
#ifndef FMTLOG_LOKI_BODY_SIZE
#define FMTLOG_LOKI_BODY_SIZE \
    (128 + FMTLOG_LOKI_BATCH * (FMTLOG_LOKI_TEXT_SIZE + 48))
#endif

// Адрес Loki вместе с путём.
#ifndef FMTLOG_LOKI_HOST_SIZE
#define FMTLOG_LOKI_HOST_SIZE 64
#endif

// Сколько ждать Loki, прежде чем считать отправку неудавшейся. Ожидание
// задерживает loop(), поэтому лучше короткое: не ушло сейчас - уйдёт со
// следующей пачкой.
#ifndef FMTLOG_LOKI_TIMEOUT_MS
#define FMTLOG_LOKI_TIMEOUT_MS 1000
#endif
