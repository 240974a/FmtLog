// История журнала: последние сообщения и независимые читатели.
//
// Записи копятся в кольце, а каждый читатель помнит своё место в нём. Чтение
// ничего не стирает, поэтому подключившийся вторым видит ту же историю, что и
// первый, - насколько её хватило.
//
//     History<2048> history;
//     history.write(record.text, record.length);
//
//     Cursor cursor;
//     history.rewind(cursor);           // читать всё, что есть
//     while(const char* p = history.read(cursor, len)) { ... }
//
// Место запоминается числом записанных байт, а не указателем: разность
// счётчиков сразу говорит, сколько читатель не выбрал и не затёрли ли уже его
// данные.
#pragma once

#include <stdint.h>
#include <string.h>

namespace fmtlog {

    // Место одного читателя в истории.
    struct Cursor {
        // Сколько байт записано к тому месту, докуда читатель дошёл.
        //
        // Счёт сквозной, а не смещение в кольце: только он позволяет отличить
        // "читатель на месте" от "читатель отстал на целый круг". Тридцати
        // двух бит хватает на месяцы работы даже подробного журнала.
        uint32_t position = 0;
        // Сколько байт потеряно: кольцо обогнало читателя и затёрло их.
        // Больше, чем вмещает история, потерять за раз нельзя, а она заведомо
        // меньше 64 КБ - поэтому здесь довольно шестнадцати бит.
        uint16_t lost = 0;
        bool active = false;
    };

    // Общая часть, не зависящая от размера: с ней работает код, которому
    // размер истории знать незачем.
    class HistoryBase {
      public:
        HistoryBase(char* storage, uint32_t capacity)
          : buf_(storage), cap_(capacity) {
        }

        // Дописывает данные в конец. Самое старое затирается молча - читатели
        // узнают о потере сами, по своему счётчику.
        void write(const char* data, uint32_t length);
        void write(char symbol) {
            write(&symbol, 1);
        }

        // Ставит читателя на самое старое, что есть в буфере.
        void rewind(Cursor& reader) const {
            reader.position = oldest();
            reader.lost = 0;
            reader.active = true;
        }

        // Ставит читателя на конец: он увидит только то, что придёт дальше.
        void seekToEnd(Cursor& reader) const {
            reader.position = written_;
            reader.lost = 0;
            reader.active = true;
        }

        // Отдаёт следующий непрочитанный кусок и двигает читателя. Возвращает
        // nullptr, когда читать нечего.
        //
        // Кусок может оборваться на краю буфера - тогда остаток придёт
        // следующим вызовом.
        const char* read(Cursor& reader, uint32_t& length) const;

        // Сколько байт читателю осталось выбрать. Если его данные затёрли,
        // здесь же он переставляется на самое старое и получает отметку о
        // потере - поэтому читатель передаётся по ссылке.
        uint32_t pending(Cursor& reader) const {
            return reader.active ? written_ - catchUp(reader) : 0;
        }

        // Сколько данных сейчас в буфере.
        uint32_t size() const {
            return written_ < cap_ ? written_ : cap_;
        }
        uint32_t capacity() const {
            return cap_;
        }
        // Всего записано с запуска; переполняется через 4 ГБ, что при типичной
        // скорости журнала - месяцы работы.
        uint32_t written() const {
            return written_;
        }

      private:
        // Номер самого старого байта, который ещё не затёрт.
        uint32_t oldest() const {
            return written_ > cap_ ? written_ - cap_ : 0;
        }

        // Возвращает место читателя, подтянув его вперёд, если буфер успел
        // затереть то, что читатель ещё не забрал.
        uint32_t catchUp(Cursor& reader) const;

        char* buf_;
        uint32_t cap_;
        uint32_t written_ = 0;
    };

    // История с памятью внутри.
    template<uint32_t Capacity>
    class History : public HistoryBase {
      public:
        History() : HistoryBase(storage_, Capacity) {
        }

      private:
        char storage_[Capacity];
    };

} // namespace fmtlog
