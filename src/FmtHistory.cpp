#include "FmtHistory.h"

namespace fmtlog {

    void HistoryBase::write(const char* data, uint32_t length) {
        if(!buf_ || !data || !length || !cap_)
            return;

        // Кусок длиннее буфера смысла хранить не имеет - берём его хвост.
        if(length > cap_) {
            data += length - cap_;
            written_ += length - cap_;
            length = cap_;
        }

        uint32_t offset = written_ % cap_;
        const uint32_t toEnd = cap_ - offset;

        if(length <= toEnd) {
            memcpy(buf_ + offset, data, length);
        }
        else {
            // Запись переходит через край: пишем двумя частями.
            memcpy(buf_ + offset, data, toEnd);
            memcpy(buf_, data + toEnd, length - toEnd);
        }
        written_ += length;
    }

    uint32_t HistoryBase::catchUp(Cursor& reader) const {
        const uint32_t first = oldest();
        // Читатель отстал настолько, что его данные уже затёрты.
        if(reader.position < first) {
            const uint32_t missed = first - reader.position;
            // За раз теряется не больше вместимости истории, но накопиться
            // за несколько отставаний может и больше - тогда счётчик замирает
            // на пределе, а не идёт по кругу.
            const uint32_t total = reader.lost + missed;
            reader.lost = total > 0xFFFFu ? 0xFFFFu : static_cast<uint16_t>(total);
            reader.position = first;
        }
        return reader.position;
    }

    const char* HistoryBase::read(Cursor& reader, uint32_t& length) const {
        length = 0;
        if(!reader.active || !buf_ || !cap_)
            return nullptr;

        const uint32_t from = catchUp(reader);
        if(from >= written_)
            return nullptr; // всё выбрано

        const uint32_t offset = from % cap_;
        uint32_t available = written_ - from;
        // Отдаём только до края буфера: остаток придёт следующим вызовом.
        const uint32_t toEnd = cap_ - offset;
        if(available > toEnd)
            available = toEnd;

        reader.position = from + available;
        length = available;
        return buf_ + offset;
    }

} // namespace fmtlog
