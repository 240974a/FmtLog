#include "FmtLokiBatch.h"

namespace fmtlog {
    namespace loki {

        namespace {
            // Дописывает кусок, следя за тем, чтобы не выйти за буфер.
            bool append(char* into, uint16_t capacity, uint16_t& at,
                        const char* text, uint16_t length) {
                if(at + length > capacity)
                    return false;
                memcpy(into + at, text, length);
                at += length;
                return true;
            }

            bool append(char* into, uint16_t capacity, uint16_t& at,
                        const char* text) {
                return append(into, capacity, at, text,
                              static_cast<uint16_t>(strlen(text)));
            }

            bool appendChar(char* into, uint16_t capacity, uint16_t& at,
                            char symbol) {
                if(at + 1 > capacity)
                    return false;
                into[at++] = symbol;
                return true;
            }

            // Беззнаковое число, старшим разрядом вперёд. Своё, потому что
            // наносекунды не помещаются в 32 бита, а тянуть ради них 64-битное
            // деление из printf на плату не хочется.
            bool appendNumber(char* into, uint16_t capacity, uint16_t& at,
                              uint64_t value) {
                char digits[20];
                uint8_t count = 0;
                do {
                    digits[count++] = static_cast<char>('0' + value % 10);
                    value /= 10;
                } while(value);
                if(at + count > capacity)
                    return false;
                while(count)
                    into[at++] = digits[--count];
                return true;
            }
        } // namespace

        bool writeJsonString(char* into, uint16_t capacity, uint16_t& at,
                             const char* text, uint16_t length) {
            for(uint16_t i = 0; i < length; ++i) {
                const char symbol = text[i];
                switch(symbol) {
                case '"':
                    if(!append(into, capacity, at, "\\\"", 2))
                        return false;
                    break;
                case '\\':
                    if(!append(into, capacity, at, "\\\\", 2))
                        return false;
                    break;
                case '\n':
                    if(!append(into, capacity, at, "\\n", 2))
                        return false;
                    break;
                case '\r':
                    if(!append(into, capacity, at, "\\r", 2))
                        return false;
                    break;
                case '\t':
                    if(!append(into, capacity, at, "\\t", 2))
                        return false;
                    break;
                default:
                    // Управляющие знаки JSON запрещает: их место занимает
                    // \u00XX. Прочие байты идут как есть - в том числе UTF-8,
                    // который так и остаётся собой.
                    if(static_cast<unsigned char>(symbol) < 0x20) {
                        static const char kHex[] = "0123456789abcdef";
                        const char escape[6] = {
                          '\\', 'u', '0', '0', kHex[(symbol >> 4) & 0x0F],
                          kHex[symbol & 0x0F]};
                        if(!append(into, capacity, at, escape, 6))
                            return false;
                    }
                    else if(!appendChar(into, capacity, at, symbol)) {
                        return false;
                    }
                }
            }
            return true;
        }

        uint64_t timestampNs(const Entry& entry) {
            if(entry.epochSeconds == 0)
                return 0;   // время неизвестно - пусть его поставит Loki
            return static_cast<uint64_t>(entry.epochSeconds) * 1000000000ULL +
                   static_cast<uint64_t>(entry.epochMillis) * 1000000ULL;
        }

        uint16_t buildBatch(char* into, uint16_t capacity, const Entry* entries,
                            uint16_t count, const Label* labels,
                            uint8_t labelCount) {
            if(count == 0)
                return 0;

            uint16_t at = 0;
            bool ok = append(into, capacity, at, "{\"streams\":[{\"stream\":{");

            // Метки приложения идут первыми: если оно задало level или source
            // по-своему, наши значения его не перебьют - Loki берёт последнее.
            for(uint8_t i = 0; ok && i < labelCount; ++i) {
                if(labels[i].name[0] == '\0')
                    continue;
                if(i)
                    ok = appendChar(into, capacity, at, ',');
                ok = ok && appendChar(into, capacity, at, '"') &&
                     writeJsonString(into, capacity, at, labels[i].name,
                                     static_cast<uint16_t>(strlen(labels[i].name))) &&
                     append(into, capacity, at, "\":\"") &&
                     writeJsonString(into, capacity, at, labels[i].value,
                                     static_cast<uint16_t>(strlen(labels[i].value))) &&
                     appendChar(into, capacity, at, '"');
            }

            // Уровень и источник - метками, а не текстом: по ним в Loki
            // отбирают строки, не разбирая сообщение.
            const char* const levelName = log::levelName(entries[0].level);
            const char* const sourceName = log::sourceName(entries[0].source);
            if(ok && labelCount)
                ok = appendChar(into, capacity, at, ',');
            ok = ok && append(into, capacity, at, "\"level\":\"") &&
                 append(into, capacity, at, levelName) &&
                 append(into, capacity, at, "\",\"source\":\"") &&
                 writeJsonString(into, capacity, at, sourceName,
                                 static_cast<uint16_t>(strlen(sourceName))) &&
                 append(into, capacity, at, "\"},\"values\":[");

            for(uint16_t i = 0; ok && i < count; ++i) {
                if(i)
                    ok = appendChar(into, capacity, at, ',');
                ok = ok && append(into, capacity, at, "[\"") &&
                     appendNumber(into, capacity, at, timestampNs(entries[i])) &&
                     append(into, capacity, at, "\",\"") &&
                     writeJsonString(into, capacity, at, entries[i].text,
                                     entries[i].textLength) &&
                     append(into, capacity, at, "\"]");
            }

            ok = ok && append(into, capacity, at, "]}]}");

            return ok ? at : 0;
        }

    } // namespace loki
} // namespace fmtlog
