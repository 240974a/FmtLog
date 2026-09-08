#include "FmtBer.h"

namespace fmtlog {

    void BerWriter::put(const uint8_t* data, uint16_t length) {
        if(at_ < length) {
            failed_ = true;
            return;
        }
        at_ -= length;
        memcpy(buf_ + at_, data, length);
    }

    void BerWriter::putLength(uint16_t length) {
        if(length < 0x80) {
            put(static_cast<uint8_t>(length));
        }
        else if(length < 0x100) {
            put(static_cast<uint8_t>(length));
            put(0x81);
        }
        else {
            put(static_cast<uint8_t>(length & 0xFF));
            put(static_cast<uint8_t>(length >> 8));
            put(0x82);
        }
    }

    void BerWriter::putInteger(int32_t value, uint8_t tag) {
        const uint16_t start = at_;
        uint32_t bits = static_cast<uint32_t>(value);
        uint8_t bytes[4];
        for(uint8_t i = 0; i < 4; ++i) {
            bytes[3 - i] = static_cast<uint8_t>(bits & 0xFF);
            bits >>= 8;
        }
        // Лишними считаются только те ведущие байты, после снятия которых знак
        // числа не меняется: у положительного старший бит следующего байта
        // должен остаться нулём, у отрицательного - единицей.
        uint8_t from = 0;
        while(from < 3) {
            const bool zeroPad = bytes[from] == 0x00 && (bytes[from + 1] & 0x80) == 0;
            const bool onesPad = bytes[from] == 0xFF && (bytes[from + 1] & 0x80) != 0;
            if(!zeroPad && !onesPad)
                break;
            ++from;
        }
        put(bytes + from, static_cast<uint16_t>(4 - from));
        close(tag, start);
    }

    void BerWriter::putString(const char* text, uint16_t length, uint8_t tag) {
        const uint16_t start = at_;
        put(reinterpret_cast<const uint8_t*>(text), length);
        close(tag, start);
    }

    bool BerWriter::parseOid(const char* dotted, uint32_t* into,
                             uint8_t& count) const {
        while(*dotted) {
            if(*dotted == '.') {
                ++dotted;
                continue;
            }
            if(*dotted < '0' || *dotted > '9')
                return false;
            uint32_t value = 0;
            while(*dotted >= '0' && *dotted <= '9')
                value = value * 10 + static_cast<uint32_t>(*dotted++ - '0');
            if(count >= FMTLOG_SNMP_OID_PARTS)
                return false;
            into[count++] = value;
        }
        return true;
    }

    void BerWriter::putBase128(uint32_t value) {
        put(static_cast<uint8_t>(value & 0x7F));
        value >>= 7;
        while(value) {
            put(static_cast<uint8_t>((value & 0x7F) | 0x80));
            value >>= 7;
        }
    }

    void BerWriter::putOid(const char* dotted, const char* suffix) {
        const uint16_t start = at_;
        uint32_t numbers[FMTLOG_SNMP_OID_PARTS];
        uint8_t count = 0;
        if(!parseOid(dotted, numbers, count) ||
           (suffix && !parseOid(suffix, numbers, count)) || count < 2) {
            failed_ = true;
            return;
        }
        // Числа укладываются от последнего к первому - буфер растёт к началу.
        for(uint8_t i = count; i > 2; --i)
            putBase128(numbers[i - 1]);
        putBase128(numbers[0] * 40 + numbers[1]);
        close(kOid, start);
    }

} // namespace fmtlog
