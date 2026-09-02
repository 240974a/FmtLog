#include "Fmt.h"

#include <stdio.h>
#include <stdlib.h>

namespace fmtlog {

    // --- запись в буфер ---------------------------------------------------

    void Fmt::write(StringView text) {
        if(!buf_ || !text.data || !text.length)
            return;
        size_t room = rest();
        if(text.length > room) {
            truncated_ = true;
            text.length = room;
        }
        if(!text.length)
            return;
        memcpy(buf_ + len_, text.data, text.length);
        len_ += text.length;
        buf_[len_] = '\0';
    }

    void Fmt::write(char symbol) {
        if(!buf_)
            return;
        if(!rest()) {
            truncated_ = true;
            return;
        }
        buf_[len_++] = symbol;
        buf_[len_] = '\0';
    }

    char* Fmt::claim(size_t needed) {
        if(!buf_ || rest() < needed) {
            truncated_ = true;
            return nullptr;
        }
        return buf_ + len_;
    }

    void Fmt::commit(size_t written) {
        len_ += written;
        if(len_ > cap_)
            len_ = cap_;
        buf_[len_] = '\0';
    }

    // --- разбор образца ---------------------------------------------------

    namespace detail {
        // Обе перегрузки различаются только чтением байта, поэтому логика
        // разбора описана один раз - иначе поведение для образца в RAM и во
        // флеше рано или поздно разъедется.
        template<typename Ptr, typename ReadByte>
        static Ptr scanPattern(Fmt& out, Ptr pattern, ReadByte at) {
            if(!pattern)
                return nullptr;

            for(size_t i = 0;; ++i) {
                const char symbol = at(i);
                if(!symbol)
                    return nullptr; // мест вставки не осталось

                const char next = at(i + 1);
                if(symbol == '{' && next == '}')
                    return pattern + i + 2; // нашли место вставки

                if((symbol == '{' && next == '{') || (symbol == '}' && next == '}')) {
                    out.write(symbol); // удвоенная скобка - печатаем одну
                    ++i;
                    continue;
                }
                out.write(symbol);
            }
        }

        const char* writeUntilPlaceholder(Fmt& out, const char* pattern) {
            return scanPattern(out, pattern, [pattern](size_t i) { return pattern[i]; });
        }

        const __FlashStringHelper* writeUntilPlaceholder(
            Fmt& out, const __FlashStringHelper* pattern) {
            const char* base = reinterpret_cast<const char*>(pattern);
            const char* tail = scanPattern(out, base, [base](size_t i) {
                return static_cast<char>(pgm_read_byte(base + i));
            });
            return reinterpret_cast<const __FlashStringHelper*>(tail);
        }
    } // namespace detail

    // --- числа ------------------------------------------------------------

    namespace {
        // Разряды выводятся в обратном порядке во временный буфер, а затем
        // переворачиваются: так не нужен ни itoa, ни деление на константу.
        template<typename U>
        void writeDigits(Fmt& out, U value, bool negative) {
            char digits[24];
            uint8_t count = 0;
            do {
                digits[count++] = static_cast<char>('0' + (value % 10));
                value /= 10;
            } while(value && count < sizeof(digits));

            if(negative)
                out.write('-');
            while(count)
                out.write(digits[--count]);
        }
    } // namespace

    void formatSigned(Fmt& out, int32_t value) {
        const bool negative = value < 0;
        // Минимальное значение по модулю не представимо в знаковом типе,
        // поэтому в беззнаковый переводим до смены знака.
        uint32_t magnitude = negative ? (~static_cast<uint32_t>(value) + 1u)
                                      : static_cast<uint32_t>(value);
        writeDigits(out, magnitude, negative);
    }

    void formatUnsigned(Fmt& out, uint32_t value) {
        writeDigits(out, value, false);
    }

    void formatSigned64(Fmt& out, int64_t value) {
        const bool negative = value < 0;
        uint64_t magnitude = negative ? (~static_cast<uint64_t>(value) + 1u)
                                      : static_cast<uint64_t>(value);
        writeDigits(out, magnitude, negative);
    }

    void formatUnsigned64(Fmt& out, uint64_t value) {
        writeDigits(out, value, false);
    }

    namespace {
        // Степени десяти для сдвига дробной части. Больше девяти знаков после
        // запятой double всё равно не различает.
        constexpr uint32_t kPow10[] = {
            1u, 10u, 100u, 1000u, 10000u,
            100000u, 1000000u, 10000000u, 100000000u, 1000000000u,
        };
        constexpr uint8_t kMaxDecimals = sizeof(kPow10) / sizeof(kPow10[0]) - 1;

        // Начиная с этого значения дробная часть double уже не хранится, и
        // раскладывать число на две целые бессмысленно.
        constexpr double kIntegerOnly = 4294967296.0; // 2^32
    } // namespace

    void formatFloat(Fmt& out, double value, uint8_t decimals) {
        if(isnan(value)) {
            formatFlashString(out, F("nan"));
            return;
        }
        if(isinf(value)) {
            formatFlashString(out, value < 0 ? F("-inf") : F("inf"));
            return;
        }
        if(decimals > kMaxDecimals)
            decimals = kMaxDecimals;

        // Знак берём у самого значения, а не сравнением: у -0.0 сравнение
        // с нулём ложно, и минус потерялся бы.
        const bool negative = signbit(value);
        if(negative)
            value = -value;

        // За этой границей у double дробной части уже нет, а в uint32 число
        // не помещается. Печатаем целую часть и дописываем нули после запятой -
        // округлять там нечего.
        if(value >= kIntegerOnly) {
            if(negative)
                out.write('-');
            writeDigits(out, static_cast<uint64_t>(value), false);
            if(decimals) {
                out.write('.');
                for(uint8_t i = 0; i < decimals; ++i)
                    out.write('0');
            }
            return;
        }

        const uint32_t scale = kPow10[decimals];

        uint32_t whole = static_cast<uint32_t>(value);
        const double rest = (value - whole) * scale;

        // Округление к ближайшему, а ровная половина - к чётному. Так же
        // округляют printf и dtostrf, иначе вывод разошёлся бы с привычным.
        //
        // Чётность смотрим у последнего печатаемого разряда: при decimals = 0
        // дробной части нет, и решает чётность целой.
        uint32_t fraction = static_cast<uint32_t>(rest);
        const double tail = rest - fraction;
        const bool lastDigitOdd = decimals ? (fraction & 1) : (whole & 1);
        if(tail > 0.5 || (tail == 0.5 && lastDigitOdd))
            ++fraction;

        // Округление могло переполнить дробную часть: 0.999 -> 1.000
        if(fraction >= scale) {
            fraction -= scale;
            ++whole;
        }

        if(negative)
            out.write('-');
        writeDigits(out, whole, false);

        if(!decimals)
            return;

        out.write('.');
        // Ведущие нули дробной части: 0.05 - это 5 при scale 100, и без них
        // напечаталось бы 0.5
        for(uint32_t limit = scale / 10; limit > 1 && fraction < limit; limit /= 10)
            out.write('0');
        writeDigits(out, fraction, false);
    }

    void formatter<bool>::format(Fmt& out, bool value) {
        formatFlashString(out, value ? F("true") : F("false"));
    }

    // --- строки -----------------------------------------------------------

    void formatFlashString(Fmt& out, const __FlashStringHelper* text) {
        if(!text)
            return;
        const char* base = reinterpret_cast<const char*>(text);
        for(size_t i = 0;; ++i) {
            const char symbol = static_cast<char>(pgm_read_byte(base + i));
            if(!symbol)
                return;
            out.write(symbol);
        }
    }

    // --- прикладные типы --------------------------------------------------

    namespace {
        void writeHexByte(Fmt& out, uint8_t value) {
            static const char kDigits[] = "0123456789ABCDEF";
            out.write(kDigits[value >> 4]);
            out.write(kDigits[value & 0x0F]);
        }

        void writeHexDump(Fmt& out, const uint8_t* data, size_t length, char delimiter) {
            for(size_t i = 0; i < length; ++i) {
                if(i && delimiter)
                    out.write(delimiter);
                writeHexByte(out, data[i]);
            }
        }

        // Двузначное число с ведущим нулём - для часов, минут и секунд.
        void writeTwoDigits(Fmt& out, uint8_t value) {
            out.write(static_cast<char>('0' + (value / 10) % 10));
            out.write(static_cast<char>('0' + value % 10));
        }
    } // namespace

    void formatter<HexDump>::format(Fmt& out, const HexDump& value) {
        writeHexDump(out, value.data, value.length, ',');
    }

    void formatter<FullDump>::format(Fmt& out, const FullDump& value) {
        if(!value.length)
            return;
        out.write('"');
        for(size_t i = 0; i < value.length; ++i) {
            const char symbol = static_cast<char>(value.data[i]);
            out.write(symbol >= 0x20 && symbol < 0x7F ? symbol : '.');
        }
        formatFlashString(out, F("\" 0x"));
        writeHexDump(out, value.data, value.length, ' ');
    }

    void formatter<Duration>::format(Fmt& out, const Duration& value) {
        int32_t ms = value.ms;
        if(ms < 0) {
            out.write('-');
            ms = -ms;
        }
        const int32_t seconds = ms / 1000;
        ms %= 1000;

        if(seconds) {
            formatSigned(out, seconds);
            out.write('s');
            if(ms)
                out.write(':');
        }
        if(ms || !seconds) {
            formatSigned(out, ms);
            formatFlashString(out, F("ms"));
        }
    }

    namespace {
        // Календарь считаем сами: gmtime есть не во всех ядрах, а тянуть <ctime>
        // ради одного вызова на AVR не хочется.
        struct CivilDate {
            uint16_t year;
            uint8_t month, day, hour, minute, second;
        };

        CivilDate civilFromEpoch(uint32_t epochSeconds) {
            CivilDate date{};
            date.second = epochSeconds % 60;
            uint32_t minutes = epochSeconds / 60;
            date.minute = minutes % 60;
            uint32_t hours = minutes / 60;
            date.hour = hours % 24;

            // Алгоритм Хауарда Хиннанта: сдвигаем начало года на март, тогда
            // високосный день оказывается последним и високосных правок внутри
            // цикла не требуется.
            int32_t days = static_cast<int32_t>(hours / 24) + 719468;
            const int32_t era = (days >= 0 ? days : days - 146096) / 146097;
            const uint32_t dayOfEra = static_cast<uint32_t>(days - era * 146097);
            const uint32_t yearOfEra
                = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
            const int32_t year = static_cast<int32_t>(yearOfEra) + era * 400;
            const uint32_t dayOfYear
                = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
            const uint32_t monthShifted = (5 * dayOfYear + 2) / 153;

            date.day = static_cast<uint8_t>(dayOfYear - (153 * monthShifted + 2) / 5 + 1);
            date.month = static_cast<uint8_t>(monthShifted < 10 ? monthShifted + 3 : monthShifted - 9);
            date.year = static_cast<uint16_t>(year + (date.month <= 2 ? 1 : 0));
            return date;
        }

        void writeTime(Fmt& out, const CivilDate& date) {
            writeTwoDigits(out, date.hour);
            out.write(':');
            writeTwoDigits(out, date.minute);
            out.write(':');
            writeTwoDigits(out, date.second);
        }
    } // namespace

    void formatter<DateTime>::format(Fmt& out, const DateTime& value) {
        const CivilDate date = civilFromEpoch(value.epochSeconds);
        writeTwoDigits(out, date.day);
        out.write('.');
        writeTwoDigits(out, date.month);
        out.write('.');
        formatUnsigned(out, date.year);
        out.write(' ');
        writeTime(out, date);
    }

    void formatter<TimeOfDay>::format(Fmt& out, const TimeOfDay& value) {
        writeTime(out, civilFromEpoch(value.epochSeconds));
    }

#if FMTLOG_HAS_IPADDRESS
    void formatter<IPAddress>::format(Fmt& out, const IPAddress& value) {
        for(uint8_t i = 0; i < 4; ++i) {
            if(i)
                out.write('.');
            formatUnsigned(out, value[i]);
        }
    }
#endif

} // namespace fmtlog
