// Журнал поверх форматирования по образцу.
//
//     using namespace fmtlog;
//
//     void setup() {
//         Serial.begin(115200);
//         log::addSink(log::serialSink);
//         log::setLevel(Level::info);
//
//         log::info(F("запуск, пин {} = {}"), pin, level);
//     }
//
// Уровнем можно управлять на ходу и раздельно по источникам: разговорчивую
// часть приложения нетрудно приглушить, не трогая остальные.
#pragma once

#include "LogConfig.h"

namespace fmtlog {

    // Форматирование пришло из FmtTiny - подтягиваем его имена, чтобы
    // приложению не приходилось писать оба пространства.
    using fmttiny::Fmt;
    using fmttiny::formatter;
    using fmttiny::StringView;
    using fmttiny::opt;
    using fmttiny::Duration;
    using fmttiny::DateTime;
    using fmttiny::DateTimeSortable;
    using fmttiny::TimeOfDay;
    using fmttiny::FixedWidth;
    using fmttiny::HexDump;
    using fmttiny::FullDump;

    // Уровни важности по возрастанию.
    //
    // Два последних не отключаются порогом: critical сообщает о том, после
    // чего работа продолжаться не может, а system - о пуске, остановке и
    // прочих вехах, которые нужны в журнале всегда.
    //
    // none в setLevel глушит всё, что ниже critical.
    enum class Level : uint8_t {
        trace, debug, info, warn, err, critical, system, none
    };

    // Отключается ли уровень порогом.
    constexpr bool isMandatory(Level level) {
        return level >= Level::critical && level != Level::none;
    }

    // Готовое сообщение, переданное приёмнику.
    struct Record {
        Level level;
        uint8_t source;
        uint32_t uptimeMs;      // millis() на момент создания записи
        uint32_t epochSeconds;  // календарное время; 0, пока оно не задано
        uint16_t epochMillis;   // доля секунды к нему
        const char* text;
        size_t length;
        bool truncated;         // сообщение не поместилось в буфер целиком
    };

    // Приёмник вывода. Библиотека сама никуда не пишет: куда попадёт
    // сообщение, решает приложение.
    using Sink = void (*)(const Record&);

    namespace log {

        // --- приёмники ----------------------------------------------------

        // Пишет в Serial: время, уровень, источник, текст. Без раскраски -
        // цветной вывод подключается отдельно, см. <FmtColor.h>
        void serialSink(const Record& record);

        // Добавляет приёмник. Их может быть несколько - например, порт и
        // собственная выдача по сети. Возвращает false, если места больше нет.
        bool addSink(Sink sink);
        void removeSink(Sink sink);
        void clearSinks();

        // --- уровни -------------------------------------------------------

        // Общий уровень для всех источников.
        void setLevel(Level level);
        // Уровень отдельного источника: перекрывает общий.
        void setLevel(uint8_t source, Level level);
        Level getLevel(uint8_t source = 0);

        // Уровни могут жить не в библиотеке, а в приложении - например в
        // EEPROM, чтобы их правили с веб-страницы. Тогда вместо хранения
        // копии библиотека спрашивает уровень у приложения, и правка
        // действует сразу.
        //
        //     log::setLevelSource([](uint8_t src) {
        //         return storedLevels[src];
        //     });
        using LevelSource = Level (*)(uint8_t source);
        void setLevelSource(LevelSource source);

        // Стоит ли вообще собирать это сообщение. Проверяется до
        // форматирования, поэтому отброшенный вызов почти ничего не стоит.
        // Уровни critical и system проходят всегда.
        bool enabled(Level level, uint8_t source = 0);

        // --- время --------------------------------------------------------

        // Сообщает библиотеке текущее календарное время: секунды эпохи Unix и
        // долю секунды. Откуда его взять - NTP, RTC или иное - решает
        // приложение; смещение часового пояса учитывать не нужно, оно уже
        // должно быть в переданном значении.
        //
        //     log::setTime(epochFromNtp);          // доля неизвестна
        //     log::setTime(epochFromNtp, 250);     // с долей секунды
        //
        // Между вызовами время досчитывается по millis(). Их кварц уходит на
        // секунды в сутки, поэтому вызов стоит повторять - раз в час или чаще,
        // смотря какая точность нужна.
        void setTime(uint32_t epochSeconds, uint16_t millisPart = 0);

        // Задано ли время. Пока нет, в записи стоит время с запуска.
        bool timeIsSet();

        // Текущее календарное время с учётом хода millis() с последнего
        // setTime. Возвращает false, пока время не задано.
        bool currentTime(uint32_t& epochSeconds, uint16_t& millisPart);

        // Имена источников для вывода. Приложение задаёт свой список; пока он
        // не задан, печатается номер.
        //
        //     const char* const kSources[] = {"app", "net", "db"};
        //     log::setSourceNames(kSources, 3);
        void setSourceNames(const char* const* names, uint8_t count);
        const char* sourceName(uint8_t source);

        // Одна буква на уровень: T, D, I, W, E, C, S.
        char levelMark(Level level);

        // Имя уровня словом: "trace", "info", "error" и так далее.
        //
        // Нужно приёмникам, чей вывод читают не только глазами: в метке
        // Prometheus или в поле JSON буква "E" ничего не говорит, а "error"
        // понятен и человеку, и системе разбора.
        const char* levelName(Level level);

        // Отметка времени записи, ширина всегда 21 знак:
        //
        //     26-09-03 12:30:45.123    время задано
        //     steady : 00000012.340    время ещё не задано, счёт с запуска
        //
        // Постоянная ширина держит столбцы на месте: строки до и после
        // синхронизации выравниваются одинаково.
        void writeTimestamp(Fmt& out, const Record& record);

        // --- запись -------------------------------------------------------

        // Собирает сообщение и раздаёт его приёмникам.
        template<typename Pattern, typename... Args>
        void write(Level level, uint8_t source, Pattern pattern, const Args&... args);

        // Уровень по умолчанию берёт источник 0 - когда источник в приложении
        // один, о них можно не думать вовсе.
#define FMTLOG_DECLARE_LEVEL(name, level)                                                          \
    template<typename Pattern, typename... Args>                                                   \
    inline void name(Pattern pattern, const Args&... args) {                                       \
        write(level, 0, pattern, args...);                                                         \
    }                                                                                              \
    template<typename Pattern, typename... Args>                                                   \
    inline void name##From(uint8_t source, Pattern pattern, const Args&... args) {                 \
        write(level, source, pattern, args...);                                                    \
    }

        FMTLOG_DECLARE_LEVEL(trace, Level::trace)
        FMTLOG_DECLARE_LEVEL(debug, Level::debug)
        FMTLOG_DECLARE_LEVEL(info, Level::info)
        FMTLOG_DECLARE_LEVEL(warn, Level::warn)
        FMTLOG_DECLARE_LEVEL(err, Level::err)
        FMTLOG_DECLARE_LEVEL(critical, Level::critical)
        FMTLOG_DECLARE_LEVEL(system, Level::system)

#undef FMTLOG_DECLARE_LEVEL

        // --- внутреннее ---------------------------------------------------

        namespace detail {
            // Общий буфер под сообщение: запись живёт до конца вызова, поэтому
            // одного хватает, а RAM экономится.
            Fmt& buffer();
            void dispatch(Level level, uint8_t source, const Fmt& message);
        } // namespace detail

    } // namespace log

    template<typename Pattern, typename... Args>
    void log::write(Level level, uint8_t source, Pattern pattern, const Args&... args) {
        // Порог, заданный при сборке, отсекает вызов целиком: ни образец, ни
        // значения в прошивку не попадают. При пороге 0 отсекать нечего, и
        // сравнение убирается совсем - иначе оно всегда ложно и компилятор
        // предупреждает об этом в каждом месте вызова.
#if FMTLOG_COMPILE_LEVEL > 0
        if(static_cast<uint8_t>(level) < FMTLOG_COMPILE_LEVEL)
            return;
#endif
        if(!enabled(level, source))
            return;

        Fmt& out = detail::buffer();
        out.clear();
        out.format(pattern, args...);
        detail::dispatch(level, source, out);
    }

} // namespace fmtlog
