#include "FmtEeprom.h"

#include <EEPROM.h>

namespace fmtlog {

    void formatter<EepromString>::format(Fmt& out, const EepromString& value) {
        for(uint16_t i = 0; i < value.maxLength; ++i) {
            const char symbol = static_cast<char>(EEPROM.read(value.address + i));
            if(!symbol)
                return; // строка кончилась раньше предела
            out.write(symbol);
        }
    }

} // namespace fmtlog
