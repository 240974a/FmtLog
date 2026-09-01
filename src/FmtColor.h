// Цветной вывод журнала в терминал.
//
//     #include <FmtLog.h>
//     #include <FmtColor.h>
//
//     log::addSink(color::serialSink);   // вместо log::serialSink
//
// Уровень и источник раскрашиваются управляющими последовательностями ANSI:
// ошибка не теряется среди отладочных строк.
//
// Модуль отдельный намеренно: приложению, которому цвет не нужен, он не
// достанется даже в виде мёртвого кода - монитор порта в Arduino IDE
// последовательностей не понимает и показывает их как мусор.
#pragma once

#include "Log.h"

namespace fmtlog {

    // Номера палитры терминала на 256 цветов.
    namespace color {

        // Оставляет обычный цвет терминала.
        constexpr uint8_t noColor = 0xFF;

        constexpr uint8_t gray = 247;
        constexpr uint8_t darkGray = 238;
        constexpr uint8_t red = 124;
        constexpr uint8_t green = 28;
        constexpr uint8_t lightGreen = 40;
        constexpr uint8_t yellow = 214;
        constexpr uint8_t blue = 33;
        constexpr uint8_t magenta = 93;
        constexpr uint8_t cyan = 45;
        constexpr uint8_t white = 15;

        // Приёмник с раскраской - замена log::serialSink.
        void serialSink(const Record& record);

        // Свой цвет каждому источнику: поток из разных частей приложения
        // читается с одного взгляда.
        //
        //     const uint8_t kColors[] = {color::white, color::cyan, color::green};
        //     color::setSourceColors(kColors, 3);
        void setSourceColors(const uint8_t* colors, uint8_t count);

        // Цвета уровней заданы заранее, но их можно заменить своими: массив на
        // пять значений, по порядку от trace до error.
        void setLevelColors(const uint8_t* colors);

        uint8_t levelColor(Level level);
        uint8_t sourceColor(uint8_t source);

        // Раскраску можно снять на ходу, не меняя приёмник.
        void setEnabled(bool enabled);
        bool enabled();

        // Печатает управляющую последовательность - для своих приёмников.
        // Значение noColor и снятая раскраска не печатают ничего.
        void apply(Print& out, uint8_t colorIndex);
        // Возвращает обычный цвет: без этого им окрасится всё, что выведется
        // дальше.
        void reset(Print& out);

    } // namespace color
} // namespace fmtlog
