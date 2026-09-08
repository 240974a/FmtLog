#include "Log.h"

namespace fmtlog {
    namespace log {

        namespace {
            // Уровень на каждый источник плюс общий, к которому источники
            // откатываются, пока свой не задан.
            constexpr uint8_t kUnset = 0xFF;

            Level commonLevel = Level::info;
            LevelSource levelSource = nullptr;
            uint8_t sourceLevels[FMTLOG_SOURCE_COUNT];
            bool sourceLevelsReady = false;

            const char* const* sourceNames = nullptr;
            uint8_t sourceNameCount = 0;

            constexpr uint8_t kMaxSinks = 4;
            Sink sinks[kMaxSinks];
            uint8_t sinkCount = 0;

            char messageBuffer[FMTLOG_MESSAGE_SIZE];

            // Календарное время: что задали и когда. Между вызовами setTime
            // время досчитывается по разности millis().
            uint32_t baseEpochSeconds = 0;
            uint16_t baseEpochMillis = 0;
            uint32_t baseUptimeMs = 0;
            bool timeKnown = false;

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
            case Level::warn: return 'W';
            case Level::err: return 'E';
            case Level::critical: return 'C';
            case Level::system: return 'S';
            default: return '?';
            }
        }

        // --- вывод --------------------------------------------------------

        void writeTimestamp(Fmt& out, const Record& record) {
            if(record.epochSeconds) {
                // 26-09-03 12:30:45.123
                out.format(F("{}.{}"), DateTimeSortable(record.epochSeconds),
                           FixedWidth(record.epochMillis, 3));
                return;
            }
            // Времени ещё нет - показываем счёт от запуска. Ширина та же, что
            // у даты, иначе столбцы разъедутся на границе синхронизации:
            //
            //     26-09-03 12:30:45.123
            //     steady : 00012340.567
            //
            // Секунды и доля разделены так же, как у даты, - точкой.
            const uint32_t seconds = record.uptimeMs / 1000u;
            const uint16_t millisPart = static_cast<uint16_t>(record.uptimeMs % 1000u);
            out.format(F("steady : {}.{}"), FixedWidth(seconds % 100000000u, 8),
                       FixedWidth(millisPart, 3));
        }

        // --- приёмники ----------------------------------------------------

        void serialSink(const Record& record) {
            char head[32];
            Fmt out(head, sizeof(head));
            writeTimestamp(out, record);
            out.format(F(" {} "), levelMark(record.level));
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

        // --- время --------------------------------------------------------

        void setTime(uint32_t epochSeconds, uint16_t millisPart) {
            baseEpochSeconds = epochSeconds;
            baseEpochMillis = millisPart % 1000;
            baseUptimeMs = millis();
            timeKnown = epochSeconds != 0;
        }

        bool timeIsSet() {
            return timeKnown;
        }

        bool currentTime(uint32_t& epochSeconds, uint16_t& millisPart) {
            if(!timeKnown)
                return false;

            // Вычитание беззнаковых верно и после переполнения millis()
            // (каждые ~49.7 суток), поэтому счёт не сбивается на границе.
            const uint32_t elapsed = millis() - baseUptimeMs;
            const uint32_t total = baseEpochMillis + elapsed;

            epochSeconds = baseEpochSeconds + total / 1000;
            millisPart = static_cast<uint16_t>(total % 1000);
            return true;
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

        void setLevelSource(LevelSource source) {
            levelSource = source;
        }

        Level getLevel(uint8_t source) {
            // Когда уровни хранит приложение, спрашиваем у него: так правка
            // действует сразу, без оповещения библиотеки.
            if(levelSource)
                return levelSource(source);
            ensureSourceLevels();
            if(source < FMTLOG_SOURCE_COUNT && sourceLevels[source] != kUnset)
                return static_cast<Level>(sourceLevels[source]);
            return commonLevel;
        }

        bool enabled(Level level, uint8_t source) {
            // Критическое и системное печатается независимо от порога.
            if(isMandatory(level))
                return true;
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

                uint32_t epochSeconds = 0;
                uint16_t epochMillis = 0;
                currentTime(epochSeconds, epochMillis);

                const Record record{level,        source,           millis(),
                                    epochSeconds, epochMillis,      message.c_str(),
                                    message.length(), message.truncated()};
                for(uint8_t i = 0; i < sinkCount; ++i)
                    sinks[i](record);
            }
        } // namespace detail

    } // namespace log
} // namespace fmtlog
