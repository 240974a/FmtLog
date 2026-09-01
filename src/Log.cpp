#include "Log.h"

namespace fmtlog {
    namespace log {

        namespace {
            // Уровень на каждый источник плюс общий, к которому источники
            // откатываются, пока свой не задан.
            constexpr uint8_t kUnset = 0xFF;

            Level commonLevel = Level::info;
            uint8_t sourceLevels[FMTLOG_SOURCE_COUNT];
            bool sourceLevelsReady = false;

            const char* const* sourceNames = nullptr;
            uint8_t sourceNameCount = 0;

            constexpr uint8_t kMaxSinks = 4;
            Sink sinks[kMaxSinks];
            uint8_t sinkCount = 0;

            char messageBuffer[FMTLOG_MESSAGE_SIZE];

            void ensureSourceLevels() {
                if(sourceLevelsReady)
                    return;
                for(uint8_t i = 0; i < FMTLOG_SOURCE_COUNT; ++i)
                    sourceLevels[i] = kUnset;
                sourceLevelsReady = true;
            }

        } // namespace

        // Одна буква на уровень: в тесном выводе она читается не хуже слова,
        // а места занимает меньше.
        char levelMark(Level level) {
            switch(level) {
            case Level::trace: return 'T';
            case Level::debug: return 'D';
            case Level::info: return 'I';
            case Level::warning: return 'W';
            case Level::error: return 'E';
            default: return '?';
            }
        }

        // --- приёмники ----------------------------------------------------

        void serialSink(const Record& record) {
            char head[24];
            Fmt out(head, sizeof(head));
            // Время работы, уровень и источник: [12s:340ms] I app:
            out.format(F("[{}] {} "), Duration(static_cast<int32_t>(record.uptimeMs)),
                       levelMark(record.level));
            Serial.print(out.c_str());
            Serial.print(sourceName(record.source));
            Serial.print(F(": "));
            Serial.print(record.text);
            // Обрезанное сообщение помечаем: иначе потеря хвоста незаметна.
            if(record.truncated)
                Serial.print(F(" ..."));
            Serial.println();
        }

        bool addSink(Sink sink) {
            if(!sink || sinkCount >= kMaxSinks)
                return false;
            for(uint8_t i = 0; i < sinkCount; ++i)
                if(sinks[i] == sink)
                    return true; // уже добавлен
            sinks[sinkCount++] = sink;
            return true;
        }

        void removeSink(Sink sink) {
            for(uint8_t i = 0; i < sinkCount; ++i) {
                if(sinks[i] != sink)
                    continue;
                for(uint8_t j = i; j + 1 < sinkCount; ++j)
                    sinks[j] = sinks[j + 1];
                --sinkCount;
                return;
            }
        }

        void clearSinks() {
            sinkCount = 0;
        }

        // --- уровни -------------------------------------------------------

        void setLevel(Level level) {
            commonLevel = level;
        }

        void setLevel(uint8_t source, Level level) {
            ensureSourceLevels();
            if(source < FMTLOG_SOURCE_COUNT)
                sourceLevels[source] = static_cast<uint8_t>(level);
        }

        Level getLevel(uint8_t source) {
            ensureSourceLevels();
            if(source < FMTLOG_SOURCE_COUNT && sourceLevels[source] != kUnset)
                return static_cast<Level>(sourceLevels[source]);
            return commonLevel;
        }

        bool enabled(Level level, uint8_t source) {
            return static_cast<uint8_t>(level) >= static_cast<uint8_t>(getLevel(source));
        }

        void setSourceNames(const char* const* names, uint8_t count) {
            sourceNames = names;
            sourceNameCount = count;
        }

        const char* sourceName(uint8_t source) {
            if(sourceNames && source < sourceNameCount && sourceNames[source])
                return sourceNames[source];
            // Имён нет - печатаем номер. Буфер статический: значение нужно лишь
            // до конца вывода строки.
            static char fallback[4];
            Fmt out(fallback, sizeof(fallback));
            out(source);
            return fallback;
        }

        // --- внутреннее ---------------------------------------------------

        namespace detail {
            Fmt& buffer() {
                static Fmt shared(messageBuffer, sizeof(messageBuffer));
                return shared;
            }

            void dispatch(Level level, uint8_t source, const Fmt& message) {
                if(message.empty())
                    return;
                const Record record{level,           source,          millis(),
                                    message.c_str(), message.length(), message.truncated()};
                for(uint8_t i = 0; i < sinkCount; ++i)
                    sinks[i](record);
            }
        } // namespace detail

    } // namespace log
} // namespace fmtlog
