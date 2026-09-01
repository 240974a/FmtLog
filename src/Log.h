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

#include "Fmt.h"

namespace fmtlog {

    // Уровни важности по возрастанию. none выключает источник целиком.
    enum class Level : uint8_t { trace, debug, info, warning, error, none };

    // Готовое сообщение, переданное приёмнику.
    struct Record {
        Level level;
        uint8_t source;
        uint32_t uptimeMs;  // millis() на момент создания записи
        const char* text;
        size_t length;
        bool truncated;     // сообщение не поместилось в буфер целиком
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

        // Стоит ли вообще собирать это сообщение. Проверяется до
        // форматирования, поэтому отброшенный вызов почти ничего не стоит.
        bool enabled(Level level, uint8_t source = 0);

        // Имена источников для вывода. Приложение задаёт свой список; пока он
        // не задан, печатается номер.
        //
        //     const char* const kSources[] = {"app", "net", "db"};
        //     log::setSourceNames(kSources, 3);
        void setSourceNames(const char* const* names, uint8_t count);
        const char* sourceName(uint8_t source);

        // Одна буква на уровень: T, D, I, W, E.
        char levelMark(Level level);

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
        FMTLOG_DECLARE_LEVEL(warning, Level::warning)
        FMTLOG_DECLARE_LEVEL(error, Level::error)

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
