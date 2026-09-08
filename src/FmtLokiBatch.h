// Тело запроса к Loki: что именно уходит в пачке.
//
// Отделено от отправки, потому что сети не требует: JSON собирается в буфер, а
// кто и куда его отправит - дело FmtLoki.cpp. Так формат проверяется тестами
// на машине разработчика, без платы и без Loki.
//
// Формат push API:
//
//     {"streams":[{"stream":{"job":"boiler","level":"error"},
//                  "values":[["1757000000123000000","pump is stuck"]]}]}
//
// Строки с разными метками попадают в разные потоки, поэтому пачка режется по
// сочетанию уровня и источника: у Loki это разные потоки и есть.
#pragma once

#include <stdint.h>
#include <string.h>

#include "Log.h"
#include "LogConfig.h"

namespace fmtlog {
    namespace loki {

        // Одна строка, ожидающая отправки.
        struct Entry {
            Level level = Level::info;
            uint8_t source = 0;
            uint32_t epochSeconds = 0;
            uint16_t epochMillis = 0;
            uint32_t uptimeMs = 0;
            uint16_t textLength = 0;
            char text[FMTLOG_LOKI_TEXT_SIZE] = {};
        };

        // Метка, общая для всех строк пачки.
        struct Label {
            char name[FMTLOG_LOKI_LABEL_SIZE] = {};
            char value[FMTLOG_LOKI_LABEL_SIZE] = {};
        };

        // Пишет строку в JSON, экранируя то, что там запрещено: кавычки,
        // обратную косую и управляющие знаки.
        //
        // Возвращает false, если не поместилось.
        bool writeJsonString(char* into, uint16_t capacity, uint16_t& at,
                             const char* text, uint16_t length);

        // Время строки в наносекундах - так их принимает Loki.
        //
        // Пока приложение не вызвало log::setTime(), настоящего времени нет.
        // Тогда возвращается 0, и строку метит временем сам Loki: сдвинуть её
        // на десятилетия назад, посчитав от запуска платы, было бы хуже.
        uint64_t timestampNs(const Entry& entry);

        // Собирает тело запроса из строк с одинаковыми метками.
        //
        // entries - подряд идущие строки одного потока (общие уровень и
        // источник). Возвращает длину тела или 0, если оно не поместилось.
        uint16_t buildBatch(char* into, uint16_t capacity, const Entry* entries,
                            uint16_t count, const Label* labels,
                            uint8_t labelCount);

    } // namespace loki
} // namespace fmtlog
