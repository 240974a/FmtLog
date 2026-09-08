#include "FmtSnmpTrap.h"

namespace fmtlog {
    namespace snmp {

        namespace {
            // Одна переменная списка: OID и значение при нём.
            //
            // Пишется как всё в BER - с конца, поэтому значение идёт раньше
            // своего OID.
            struct Variable {
                BerWriter& out;
                uint16_t start;

                explicit Variable(BerWriter& writer)
                  : out(writer), start(writer.mark()) {
                }
                ~Variable() {
                    out.close(BerWriter::kSequence, start);
                }
            };
        } // namespace

        uint16_t buildTrap(uint8_t* buffer, uint16_t capacity, const Notification& notification,
                           const TrapSettings& settings, uint32_t uptimeTicks) {
            BerWriter out(buffer, capacity);

            const uint16_t pduStart = out.mark();

            // Свои переменные - в обратном порядке, буфер растёт к началу.
            {
                Variable var(out);
                out.putCString(log::sourceName(notification.source));
                out.putOid(settings.enterpriseOid, "1.3");
            }
            {
                Variable var(out);
                out.putInteger(static_cast<int32_t>(notification.level));
                out.putOid(settings.enterpriseOid, "1.2");
            }
            {
                Variable var(out);
                out.putString(notification.text, notification.textLength);
                out.putOid(settings.enterpriseOid, "1.1");
            }

            // snmpTrapOID.0 - что именно случилось. Ветка отправителя плюс
            // номер уровня: приёмник различает события, не читая текст.
            {
                Variable var(out);
                char suffix[8];
                Fmt mark(suffix, sizeof(suffix));
                mark(static_cast<uint32_t>(notification.level));
                out.putOid(settings.enterpriseOid, mark.c_str());
                out.putOid("1.3.6.1.6.3.1.1.4.1.0");
            }

            // sysUpTime.0 - сотые доли секунды с запуска, тип TimeTicks.
            {
                Variable var(out);
                out.putInteger(static_cast<int32_t>(uptimeTicks),
                               BerWriter::kTimeTicks);
                out.putOid("1.3.6.1.2.1.1.3.0");
            }

            out.close(BerWriter::kSequence, pduStart);   // список переменных

            out.putInteger(0);   // error-index
            out.putInteger(0);   // error-status
            out.putInteger(0);   // request-id: ответа не ждём
            out.close(BerWriter::kTrapV2, pduStart);

            out.putCString(settings.community);
            out.putInteger(1);   // version: 1 означает SNMPv2c

            out.close(BerWriter::kSequence, capacity);   // всё вместе

            if(out.failed())
                return 0;

            const uint16_t length = out.size();
            memmove(buffer, out.data(), length);
            return length;
        }

    } // namespace snmp
} // namespace fmtlog
