#include "FmtColor.h"

namespace fmtlog {
    namespace color {

        namespace {
            const uint8_t* sourceColors = nullptr;
            uint8_t sourceColorCount = 0;
            bool colorOn = true;

            // Тревожное - тёплое и яркое, будничное - тусклое. Ни один не
            // совпадает с цветом времени, иначе строка сливается в одно пятно
            // и уровень перестаёт читаться.
            constexpr uint8_t kDefaultLevelColors[] = {
                gray,       // trace
                blue,       // debug
                lightGreen, // info
                yellow,     // warning
                red,        // error
            };
            constexpr uint8_t kLevelCount
                = sizeof(kDefaultLevelColors) / sizeof(kDefaultLevelColors[0]);

            const uint8_t* levelColors = kDefaultLevelColors;

            // Время печатается тусклым: оно нужно для отсчёта, а не для чтения.
            constexpr uint8_t kTimeColor = darkGray;
        } // namespace

        void setSourceColors(const uint8_t* colors, uint8_t count) {
            sourceColors = colors;
            sourceColorCount = count;
        }

        void setLevelColors(const uint8_t* colors) {
            levelColors = colors ? colors : kDefaultLevelColors;
        }

        uint8_t levelColor(Level level) {
            const uint8_t index = static_cast<uint8_t>(level);
            return index < kLevelCount ? levelColors[index] : noColor;
        }

        uint8_t sourceColor(uint8_t source) {
            if(sourceColors && source < sourceColorCount)
                return sourceColors[source];
            return noColor;
        }

        void setEnabled(bool value) {
            colorOn = value;
        }

        bool enabled() {
            return colorOn;
        }

        // Управляющая последовательность ANSI: \033[38;5;<цвет>m
        void apply(Print& out, uint8_t colorIndex) {
            if(!colorOn || colorIndex == noColor)
                return;
            out.print(F("\033[38;5;"));
            out.print(colorIndex);
            out.print('m');
        }

        void reset(Print& out) {
            if(colorOn)
                out.print(F("\033[0m"));
        }

        void serialSink(const Record& record) {
            char head[24];
            Fmt out(head, sizeof(head));

            apply(Serial, kTimeColor);
            out.format(F("[{}] "), Duration(static_cast<int32_t>(record.uptimeMs)));
            Serial.print(out.c_str());

            // Уровень задаёт цвет строки, источник печатается своим.
            const uint8_t lineColor = levelColor(record.level);
            apply(Serial, lineColor);
            Serial.print(log::levelMark(record.level));
            Serial.print(' ');

            apply(Serial, sourceColor(record.source));
            Serial.print(log::sourceName(record.source));

            apply(Serial, lineColor);
            Serial.print(F(": "));
            Serial.print(record.text);

            // Обрезанное сообщение помечаем: иначе потеря хвоста незаметна.
            if(record.truncated) {
                apply(Serial, red);
                Serial.print(F(" ..."));
            }
            reset(Serial);
            Serial.println();
        }

    } // namespace color
} // namespace fmtlog
