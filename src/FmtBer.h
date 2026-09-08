// Кодирование BER - то немногое из ASN.1, что нужно для отправки trap.
//
// Пакет собирается в буфер с конца к началу: длина каждой обёртки известна
// только после того, как записано её содержимое, а укладывая задом наперёд,
// её не приходится ни угадывать, ни потом двигать уже записанное.
//
//     uint8_t buffer[256];
//     BerWriter w(buffer, sizeof(buffer));
//     const uint16_t start = w.mark();
//     w.putCString("boiler");
//     w.putOid("1.3.6.1.3.1.1.1");
//     w.close(BerWriter::kSequence, start);   // обернуть записанное
//
//     w.data(), w.size()                      // готовый кусок
//
// Разбора здесь нет вовсе: плата только шлёт.
//
// Сети файл не требует, поэтому собирается всюду - и проверяется тестами на
// машине разработчика.
#pragma once

#include <stdint.h>
#include <string.h>

#include "LogConfig.h"

namespace fmtlog {

    class BerWriter {
      public:
        // Метки типов, нужные trap.
        static constexpr uint8_t kInteger = 0x02;
        static constexpr uint8_t kString = 0x04;
        static constexpr uint8_t kOid = 0x06;
        static constexpr uint8_t kSequence = 0x30;
        static constexpr uint8_t kTimeTicks = 0x43;
        static constexpr uint8_t kTrapV2 = 0xA7;

        BerWriter(uint8_t* buffer, uint16_t capacity)
          : buf_(buffer), at_(capacity), cap_(capacity) {
        }

        // Не хватило места или не разобрался OID. Проверять довольно один раз
        // в конце: после отказа записи не делаются.
        bool failed() const {
            return failed_;
        }

        // Начало записанного и его длина.
        const uint8_t* data() const {
            return buf_ + at_;
        }
        uint16_t size() const {
            return failed_ ? 0 : static_cast<uint16_t>(cap_ - at_);
        }

        // Место, снятое до записи содержимого: его передают в close().
        uint16_t mark() const {
            return at_;
        }

        // Закрывает поле: длина того, что записано после метки, и сама метка.
        void close(uint8_t tag, uint16_t mark) {
            putLength(static_cast<uint16_t>(mark - at_));
            put(tag);
        }

        // Целое со знаком, старшим байтом вперёд. Ведущие байты, не несущие
        // значения, отбрасываются - как того требует BER.
        void putInteger(int32_t value, uint8_t tag = kInteger);

        // Длина задаётся явно: в сообщении журнала может быть ноль, и
        // обрывать по нему нельзя.
        void putString(const char* text, uint16_t length, uint8_t tag = kString);

        // То же для строки, заканчивающейся нулём. Имя другое нарочно: с
        // одним putString вызов putString(text, 5) читался бы как длина, а
        // понимался бы как метка типа - ошибка, которую не видно глазом.
        void putCString(const char* text, uint8_t tag = kString) {
            putString(text, static_cast<uint16_t>(strlen(text)), tag);
        }

        // OID из строки вида "1.3.6.1.4.1.12345". Первые два числа по правилам
        // BER складываются в один байт, остальные пишутся по семь бит с
        // продолжением.
        //
        // suffix дописывается к строке через точку - им обозначают конкретную
        // переменную внутри своей ветки.
        void putOid(const char* dotted, const char* suffix = nullptr);

      private:
        // Один байт в начало уже записанного.
        void put(uint8_t byte) {
            if(at_ == 0) {
                failed_ = true;
                return;
            }
            buf_[--at_] = byte;
        }

        void put(const uint8_t* data, uint16_t length);

        // Длина в форме BER: до 127 - одним байтом, дальше - счётчик байт со
        // старшим битом.
        void putLength(uint16_t length);

        // Разбирает "1.3.6" в числа, дописывая их к уже разобранным.
        bool parseOid(const char* dotted, uint32_t* into, uint8_t& count) const;

        // Число по семь бит на байт, старший бит - признак продолжения.
        // Последний байт идёт без него, поэтому пишется первым: буфер
        // укладывается с конца.
        void putBase128(uint32_t value);

        uint8_t* buf_;
        uint16_t at_;
        uint16_t cap_;
        bool failed_ = false;
    };

} // namespace fmtlog
