// Сборка trap SNMPv2c: что именно уходит в пакете.
//
// Отделено от отправки, потому что сети не требует: пакет собирается в буфер,
// а кто и куда его отправит - дело FmtSnmp.cpp. Так содержимое trap
// проверяется тестами на машине разработчика, без платы и коллектора.
#pragma once

#include <stdint.h>
#include <string.h>

#include "FmtBer.h"
#include "Log.h"

namespace fmtlog {
    namespace snmp {

        // Событие, ожидающее отправки.
        struct Notification {
            Level level = Level::err;
            uint8_t source = 0;
            uint16_t textLength = 0;
            char text[FMTLOG_SNMP_TEXT_SIZE] = {};
        };

        // От чьего имени и в какой ветке OID слать.
        struct TrapSettings {
            // Пароль в открытую, каким он в SNMPv2c и задуман.
            char community[FMTLOG_SNMP_COMMUNITY_SIZE] = "public";
            // Ветка enterprises.experimental: свободна для проб, своим
            // устройствам полагается своя.
            char enterpriseOid[FMTLOG_SNMP_OID_SIZE] = "1.3.6.1.3.1";

            void setCommunity(const char* value) {
                copyInto(community, sizeof(community), value);
            }
            void setEnterpriseOid(const char* value) {
                copyInto(enterpriseOid, sizeof(enterpriseOid), value);
            }

          private:
            static void copyInto(char* into, size_t size, const char* value) {
                if(!value)
                    return;
                strncpy(into, value, size - 1);
                into[size - 1] = '\0';
            }
        };

        // Собирает trap в конец буфера и переносит в начало. Возвращает длину
        // пакета или 0, если он не поместился.
        //
        // Порядок переменных задан стандартом: сначала время работы, потом OID
        // самого trap, а дальше - что кладёт отправитель.
        //
        //     <enterprise>.1.1   текст сообщения
        //     <enterprise>.1.2   номер уровня
        //     <enterprise>.1.3   имя источника
        //
        // По уровню и источнику фильтруют на стороне приёмника, не разбирая
        // текст.
        uint16_t buildTrap(uint8_t* buffer, uint16_t capacity, const Notification& notification,
                           const TrapSettings& settings, uint32_t uptimeTicks);

    } // namespace snmp
} // namespace fmtlog
