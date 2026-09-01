// Вывод строк, лежащих в EEPROM.
//
//     log::info(F("имя устройства: {}"), EepromString(0, 32));
//
// Заголовок отдельный: EEPROM нужна не всякому приложению, а на ESP32 её
// эмуляция ещё и требует begin(). Включайте его только там, где нужен вывод
// из EEPROM.
#pragma once

#include "Fmt.h"

namespace fmtlog {

    // Строка в EEPROM: адрес и предельная длина. Чтение прекращается на
    // завершающем нуле или по достижении предела - что раньше.
    struct EepromString {
        uint16_t address = 0;
        uint16_t maxLength = 0;

        EepromString(uint16_t addr, uint16_t limit) : address(addr), maxLength(limit) {
        }
    };

    template<>
    struct formatter<EepromString> {
        static void format(Fmt& out, const EepromString& value);
    };

} // namespace fmtlog
