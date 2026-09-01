// Форматирование строки по образцу для микроконтроллеров.
//
//     char buf[64];
//     fmtlog::Fmt out(buf, sizeof(buf));
//     out.format(F("pin {} = {}"), pin, level);
//
// Место каждого значения помечено {}. Текст сообщения виден целиком и не
// разорван знаками вставки.
//
// Буфер задаётся снаружи и не растёт: если сообщение длиннее, оно обрезается
// по границе, а не выходит за неё. Ни одной динамической памяти библиотека не
// занимает.
#pragma once

#include <Arduino.h>

#include <stdint.h>
#include <string.h>

#include "FmtConfig.h"

namespace fmtlog {

    // Строка известной длины без завершающего нуля: позволяет выводить кусок
    // чужого буфера, ничего не копируя.
    struct StringView {
        const char* data = nullptr;
        size_t length = 0;

        StringView() = default;
        StringView(const char* text, size_t len) : data(text), length(len) {
        }
        // Не explicit намеренно: строковый литерал должен подставляться сам.
        StringView(const char* text) : data(text), length(text ? strlen(text) : 0) {
        }
    };

    // Дамп области памяти шестнадцатеричными парами: 0A,FF,12
    struct HexDump {
        const uint8_t* data = nullptr;
        size_t length = 0;

        HexDump(const void* ptr, size_t len)
          : data(static_cast<const uint8_t*>(ptr)), length(len) {
        }
    };

    // То же, но перед дампом печатаются исходные байты как текст: удобно, когда
    // в буфере ожидается строка, а пришло неизвестно что.
    struct FullDump : HexDump {
        using HexDump::HexDump;
    };

    // Длительность в миллисекундах: печатается как 1s:250ms, 90ms, 12s
    struct Duration {
        int32_t ms = 0;

        explicit Duration(int32_t milliseconds) : ms(milliseconds) {
        }
    };

    // Момент времени как календарная дата: 31.12.2025 23:59:59
    // Принимает время эпохи Unix в секундах.
    struct DateTime {
        uint32_t epochSeconds = 0;

        explicit DateTime(uint32_t seconds) : epochSeconds(seconds) {
        }
    };

    // Только время суток из того же значения: 23:59:59
    struct TimeOfDay {
        uint32_t epochSeconds = 0;

        explicit TimeOfDay(uint32_t seconds) : epochSeconds(seconds) {
        }
    };

    // ---------------------------------------------------------------------
    // Приёмник текста
    // ---------------------------------------------------------------------

    // Буфер, в который собирается сообщение.
    //
    // Владеет только указателем: память даёт вызывающий, поэтому буфер может
    // быть и на стеке, и общим на всё приложение.
    class Fmt {
      public:
        Fmt(char* buffer, size_t capacity) : buf_(buffer), cap_(capacity ? capacity - 1 : 0) {
            if(buf_ && capacity)
                buf_[0] = '\0';
        }

        // Собирает сообщение по образцу, заменяя каждое {} очередным значением.
        // Возвращает себя, поэтому вызовы можно продолжать.
        template<typename... Args>
        Fmt& format(const char* pattern, const Args&... args);

        // Образец во флеше: format(F("pin {}"), pin). На AVR и ESP это
        // экономит RAM, потому что образец в неё не копируется.
        template<typename... Args>
        Fmt& format(const __FlashStringHelper* pattern, const Args&... args);

        // Дописывает одно значение без образца.
        template<typename T>
        Fmt& operator()(const T& value);

        const char* c_str() const {
            return buf_ ? buf_ : "";
        }
        size_t length() const {
            return len_;
        }
        bool empty() const {
            return len_ == 0;
        }
        // Сообщение не поместилось целиком и было обрезано.
        bool truncated() const {
            return truncated_;
        }
        void clear() {
            len_ = 0;
            truncated_ = false;
            if(buf_)
                buf_[0] = '\0';
        }

        // Сколько ещё поместится.
        size_t rest() const {
            return len_ >= cap_ ? 0 : cap_ - len_;
        }

        // Единственные две точки, через которые текст попадает в буфер:
        // здесь же происходит обрезка по границе.
        void write(StringView text);
        void write(char symbol);
        // Запись напрямую в хвост буфера - для форматтеров, которые пишут сами.
        // Возвращает место под запись или nullptr, если его не осталось.
        char* claim(size_t needed);
        void commit(size_t written);

      private:
        char* buf_ = nullptr;
        size_t cap_ = 0;
        size_t len_ = 0;
        bool truncated_ = false;
    };

    // ---------------------------------------------------------------------
    // Подключение типов
    // ---------------------------------------------------------------------
    //
    // Каждый тип печатается своей специализацией formatter<T>. Чтобы научить
    // библиотеку новому типу, довольно объявить
    //
    //     template<> struct fmtlog::formatter<MyType> {
    //         static void format(Fmt& out, const MyType& value);
    //     };
    //
    // Частичная специализация тоже работает: ниже так описаны сразу все целые,
    // все дробные и все перечисления.
    template<typename T, typename Enable = void>
    struct formatter;

    namespace detail {
        template<typename T>
        struct Bare {
            using type = T;
        };
        template<typename T>
        struct Bare<const T> {
            using type = typename Bare<T>::type;
        };
        template<typename T>
        struct Bare<T&> {
            using type = typename Bare<T>::type;
        };

        // Сюда сходятся все места, где значение попадает в буфер.
        template<typename T>
        inline void formatValue(Fmt& out, const T& value) {
            formatter<typename Bare<T>::type>::format(out, value);
        }
    } // namespace detail

    // --- числа -----------------------------------------------------------
    //
    // Все знаковые целые печатаются через int32_t, все беззнаковые - через
    // uint32_t: одна пара функций обслуживает и char, и int, и long.
    void formatSigned(Fmt& out, int32_t value);
    void formatUnsigned(Fmt& out, uint32_t value);
    void formatSigned64(Fmt& out, int64_t value);
    void formatUnsigned64(Fmt& out, uint64_t value);
    // Дробное с фиксированным числом знаков после запятой.
    void formatFloat(Fmt& out, double value, uint8_t decimals = FMTLOG_FLOAT_DECIMALS);

    template<typename T>
    struct formatter<
        T, typename std::enable_if<
               std::is_integral<T>::value && std::is_signed<T>::value
               && !std::is_same<T, char>::value && !std::is_same<T, bool>::value
               && (sizeof(T) <= 4)>::type> {
        static void format(Fmt& out, T value) {
            formatSigned(out, static_cast<int32_t>(value));
        }
    };
    template<typename T>
    struct formatter<
        T, typename std::enable_if<
               std::is_integral<T>::value && std::is_unsigned<T>::value
               && !std::is_same<T, bool>::value && (sizeof(T) <= 4)>::type> {
        static void format(Fmt& out, T value) {
            formatUnsigned(out, static_cast<uint32_t>(value));
        }
    };
    template<typename T>
    struct formatter<
        T, typename std::enable_if<
               std::is_integral<T>::value && std::is_signed<T>::value && (sizeof(T) > 4)>::type> {
        static void format(Fmt& out, T value) {
            formatSigned64(out, static_cast<int64_t>(value));
        }
    };
    template<typename T>
    struct formatter<
        T, typename std::enable_if<
               std::is_integral<T>::value && std::is_unsigned<T>::value && (sizeof(T) > 4)>::type> {
        static void format(Fmt& out, T value) {
            formatUnsigned64(out, static_cast<uint64_t>(value));
        }
    };
    template<typename T>
    struct formatter<T, typename std::enable_if<std::is_floating_point<T>::value>::type> {
        static void format(Fmt& out, T value) {
            formatFloat(out, static_cast<double>(value));
        }
    };
    // Перечисление печатается своим численным значением.
    template<typename T>
    struct formatter<T, typename std::enable_if<std::is_enum<T>::value>::type> {
        static void format(Fmt& out, T value) {
            formatSigned(out, static_cast<int32_t>(value));
        }
    };

    template<>
    struct formatter<bool> {
        static void format(Fmt& out, bool value);
    };
    template<>
    struct formatter<char> {
        static void format(Fmt& out, char value) {
            out.write(value);
        }
    };

    // --- строки ----------------------------------------------------------

    void formatFlashString(Fmt& out, const __FlashStringHelper* text);

    template<>
    struct formatter<const char*> {
        static void format(Fmt& out, const char* value) {
            if(value)
                out.write(StringView{value});
        }
    };
    template<>
    struct formatter<char*> {
        static void format(Fmt& out, const char* value) {
            if(value)
                out.write(StringView{value});
        }
    };
    // Строковый литерал имеет тип char[N]; без этой специализации он был бы
    // разобран как массив, а не как строка.
    template<size_t N>
    struct formatter<char[N]> {
        static void format(Fmt& out, const char* value) {
            out.write(StringView{value});
        }
    };
    template<>
    struct formatter<StringView> {
        static void format(Fmt& out, StringView value) {
            out.write(value);
        }
    };
    template<>
    struct formatter<const __FlashStringHelper*> {
        static void format(Fmt& out, const __FlashStringHelper* value) {
            formatFlashString(out, value);
        }
    };
    template<>
    struct formatter<__FlashStringHelper*> {
        static void format(Fmt& out, const __FlashStringHelper* value) {
            formatFlashString(out, value);
        }
    };
    template<>
    struct formatter<String> {
        static void format(Fmt& out, const String& value) {
            out.write(StringView{value.c_str(), value.length()});
        }
    };

    // --- прикладные типы --------------------------------------------------

    template<>
    struct formatter<HexDump> {
        static void format(Fmt& out, const HexDump& value);
    };
    template<>
    struct formatter<FullDump> {
        static void format(Fmt& out, const FullDump& value);
    };
    template<>
    struct formatter<Duration> {
        static void format(Fmt& out, const Duration& value);
    };
    template<>
    struct formatter<DateTime> {
        static void format(Fmt& out, const DateTime& value);
    };
    template<>
    struct formatter<TimeOfDay> {
        static void format(Fmt& out, const TimeOfDay& value);
    };

#if FMTLOG_HAS_IPADDRESS
    template<>
    struct formatter<IPAddress> {
        static void format(Fmt& out, const IPAddress& value);
    };
#endif

    // --- необязательное значение ------------------------------------------
    //
    // Значение, которое печатается только при выполненном условии:
    //
    //     log.info(F("команда:'{}'{}"), name, opt(hasValue, F(" = "), value));
    //
    // Когда условие ложно, место вставки остаётся пустым - ни префикс, ни само
    // значение, ни суффикс не выводятся. Условие при этом пишется один раз.
    template<typename Prefix, typename T, typename Suffix>
    struct OptValue {
        bool present;
        Prefix prefix;
        const T& value;
        Suffix suffix;
    };

    // Признак «печатать нечего»: подставляется вместо отсутствующего префикса
    // или суффикса и ничего не выводит.
    struct Nothing {};

    // opt(условие, значение)
    template<typename T>
    inline OptValue<Nothing, T, Nothing> opt(bool present, const T& value) {
        return OptValue<Nothing, T, Nothing>{present, Nothing{}, value, Nothing{}};
    }
    // opt(условие, префикс, значение)
    template<typename Prefix, typename T>
    inline OptValue<Prefix, T, Nothing> opt(bool present, Prefix prefix, const T& value) {
        return OptValue<Prefix, T, Nothing>{present, prefix, value, Nothing{}};
    }
    // opt(условие, префикс, значение, суффикс)
    template<typename Prefix, typename T, typename Suffix>
    inline OptValue<Prefix, T, Suffix> opt(
        bool present, Prefix prefix, const T& value, Suffix suffix) {
        return OptValue<Prefix, T, Suffix>{present, prefix, value, suffix};
    }

    template<>
    struct formatter<Nothing> {
        static void format(Fmt&, Nothing) {
        }
    };
    template<typename Prefix, typename T, typename Suffix>
    struct formatter<OptValue<Prefix, T, Suffix>> {
        static void format(Fmt& out, const OptValue<Prefix, T, Suffix>& value) {
            if(!value.present)
                return;
            detail::formatValue(out, value.prefix);
            detail::formatValue(out, value.value);
            detail::formatValue(out, value.suffix);
        }
    };

    // ---------------------------------------------------------------------
    // Разбор образца
    // ---------------------------------------------------------------------

    namespace detail {
        // Выводит образец до ближайшего {} и возвращает продолжение за ним.
        // Возвращает nullptr, когда мест вставки больше нет: хвост образца к
        // этому моменту уже выведен.
        //
        // Удвоенные скобки {{ и }} печатают одну: так в сообщение попадает сама
        // фигурная скобка, а не место вставки.
        const char* writeUntilPlaceholder(Fmt& out, const char* pattern);
        const __FlashStringHelper* writeUntilPlaceholder(
            Fmt& out, const __FlashStringHelper* pattern);

        // Значения кончились - дочищаем хвост образца: оставшиеся {} должны
        // исчезнуть, а удвоенные скобки схлопнуться.
        template<typename Pattern>
        void formatNext(Fmt& out, Pattern pattern) {
            while((pattern = writeUntilPlaceholder(out, pattern)) != nullptr)
                ;
        }

        // Значения подставляются рекурсией по списку: так в прошивку попадают
        // правила печати только тех типов, которые действительно встречаются в
        // вызовах.
        template<typename Pattern, typename T, typename... Rest>
        void formatNext(Fmt& out, Pattern pattern, const T& value, const Rest&... rest) {
            Pattern tail = writeUntilPlaceholder(out, pattern);
            if(!tail)
                return; // мест вставки не осталось - остальные значения не нужны
            formatValue(out, value);
            formatNext(out, tail, rest...);
        }
    } // namespace detail

    template<typename... Args>
    Fmt& Fmt::format(const char* pattern, const Args&... args) {
        detail::formatNext(*this, pattern, args...);
        return *this;
    }

    template<typename... Args>
    Fmt& Fmt::format(const __FlashStringHelper* pattern, const Args&... args) {
        detail::formatNext(*this, pattern, args...);
        return *this;
    }

    template<typename T>
    Fmt& Fmt::operator()(const T& value) {
        detail::formatValue(*this, value);
        return *this;
    }

} // namespace fmtlog
