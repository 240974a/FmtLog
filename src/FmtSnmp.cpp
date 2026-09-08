// Модуль собирается только там, где есть сеть. На прочих платах файл пуст:
// иначе он ломал бы сборку приложению, которое SNMP и не включает - в
// библиотеке Arduino компилируются все исходники подряд.
//
// Само кодирование пакета сети не требует и живёт отдельно - в FmtBer.h и
// FmtSnmpTrap.h, поэтому проверяется тестами на машине разработчика.
#if defined(ESP8266) || defined(ESP32)

#include "FmtSnmp.h"

#include <WiFiUdp.h>

#include "FmtSnmpTrap.h"

namespace fmtlog {
    namespace snmp {

        namespace {
            // sink() зовётся изнутри log::err(), поэтому только запоминает
            // событие; шлёт его handle(). Очередь короткая: такие события редки, а
            // если сеть лежит дольше, чем она вмещает, дежурному нужнее
            // свежие.
            Notification queue[FMTLOG_SNMP_QUEUE];
            uint8_t queued = 0;
            uint16_t lostCount = 0;

            WiFiUDP* udp = nullptr;
            IPAddress receiverAddress;
            uint16_t receiverPort = 162;
            TrapSettings settings;
            Level threshold = Level::err;

            // Снимает первую запись, сдвигая очередь.
            void dropFirst() {
                for(uint8_t i = 0; i + 1 < queued; ++i)
                    queue[i] = queue[i + 1];
                if(queued)
                    --queued;
            }

            void countLost() {
                if(lostCount < 0xFFFF)
                    ++lostCount;
            }
        } // namespace

        void sink(const Record& record) {
            if(record.level < threshold)
                return;

            if(queued >= FMTLOG_SNMP_QUEUE) {
                // Очередь полна - теряем самое старое: дежурному нужнее то,
                // что происходит сейчас.
                countLost();
                dropFirst();
            }

            Notification& into = queue[queued++];
            into.level = record.level;
            into.source = record.source;
            uint16_t length = static_cast<uint16_t>(record.length);
            if(length > sizeof(into.text))
                length = sizeof(into.text);
            memcpy(into.text, record.text, length);
            into.textLength = length;
        }

        void begin(IPAddress receiver, const char* community, uint16_t port) {
            end();
            receiverAddress = receiver;
            receiverPort = port;
            settings.setCommunity(community);
            udp = new WiFiUDP();
            udp->begin(0);   // порт отправителя выбирает система
        }

        void end() {
            if(udp) {
                udp->stop();
                delete udp;
                udp = nullptr;
            }
            queued = 0;
        }

        void setLevel(Level level) {
            threshold = level;
        }

        void setEnterpriseOid(const char* dotted) {
            settings.setEnterpriseOid(dotted);
        }

        uint16_t lost() {
            return lostCount;
        }

        uint8_t pending() {
            return queued;
        }

        void handle() {
            if(!udp || queued == 0)
                return;

            // За проход уходит один trap: их немного, а loop() задерживать
            // нельзя.
            uint8_t packet[FMTLOG_SNMP_PACKET_SIZE];
            const uint32_t ticks = millis() / 10;   // сотые доли секунды
            const uint16_t length =
              buildTrap(packet, sizeof(packet), queue[0], settings, ticks);

            if(length && udp->beginPacket(receiverAddress, receiverPort)) {
                udp->write(packet, length);
                udp->endPacket();
            }
            else {
                // Не собрался пакет или не ушёл - запись всё равно снимаем:
                // копить её значило бы держать очередь занятой тем, что не
                // уйдёт и в следующий раз.
                countLost();
            }
            dropFirst();
        }

    } // namespace snmp
} // namespace fmtlog

#endif // ESP8266 || ESP32
