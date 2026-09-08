// Кодирование BER и содержимое trap.
//
// Пакет проверяется разбором, а не сверкой байт вслепую: так тест говорит,
// что именно разошлось, и не ломается от безобидной перестановки полей.

#include <unity.h>

#include <string>
#include <vector>

#include "FmtBer.h"
#include "FmtSnmpTrap.h"

using namespace fmtlog;
using namespace fmtlog::snmp;

namespace {

    // --- разбор -----------------------------------------------------------
    //
    // Читатель ровно того подмножества BER, которое пишет BerWriter. Держим
    // его отдельно от библиотеки: плата разбирать ничего не должна, а тесту
    // без разбора не проверить, что записано.
    struct Field {
        uint8_t tag = 0;
        std::vector<uint8_t> value;
    };

    class Reader {
      public:
        Reader(const uint8_t* data, uint16_t length)
          : at_(data), end_(data + length) {
        }

        bool done() const {
            return at_ >= end_;
        }

        // Читает следующее поле. Возвращает false, если данные оборваны.
        bool next(Field& into) {
            if(end_ - at_ < 2)
                return false;
            into.tag = *at_++;
            uint32_t length = *at_++;
            if(length & 0x80) {
                const uint8_t count = length & 0x7F;
                if(count == 0 || count > 2 || end_ - at_ < count)
                    return false;
                length = 0;
                for(uint8_t i = 0; i < count; ++i)
                    length = (length << 8) | *at_++;
            }
            if(static_cast<uint32_t>(end_ - at_) < length)
                return false;
            into.value.assign(at_, at_ + length);
            at_ += length;
            return true;
        }

      private:
        const uint8_t* at_;
        const uint8_t* end_;
    };

    // Целое из содержимого поля.
    int32_t toInteger(const std::vector<uint8_t>& value) {
        int32_t result = value.empty() || !(value[0] & 0x80) ? 0 : -1;
        for(uint8_t byte : value)
            result = (result << 8) | byte;
        return result;
    }

    std::string toText(const std::vector<uint8_t>& value) {
        return std::string(value.begin(), value.end());
    }

    // OID обратно в строку с точками.
    std::string toOid(const std::vector<uint8_t>& value) {
        if(value.empty())
            return "";
        std::string out = std::to_string(value[0] / 40) + "." +
                          std::to_string(value[0] % 40);
        uint32_t number = 0;
        for(size_t i = 1; i < value.size(); ++i) {
            number = (number << 7) | (value[i] & 0x7F);
            if(!(value[i] & 0x80)) {
                out += "." + std::to_string(number);
                number = 0;
            }
        }
        return out;
    }

    // Одна переменная trap: OID и значение при нём.
    struct Bound {
        std::string oid;
        Field value;
    };

    // Разбирает пакет до списка переменных.
    struct Trap {
        int32_t version = -1;
        std::string community;
        uint8_t pduTag = 0;
        int32_t requestId = -1;
        int32_t errorStatus = -1;
        int32_t errorIndex = -1;
        std::vector<Bound> bindings;
        bool valid = false;
    };

    Trap parse(const uint8_t* packet, uint16_t length) {
        Trap trap;
        Reader outer(packet, length);
        Field envelope;
        if(!outer.next(envelope) || envelope.tag != BerWriter::kSequence)
            return trap;

        Reader body(envelope.value.data(),
                    static_cast<uint16_t>(envelope.value.size()));
        Field field;
        if(!body.next(field) || field.tag != BerWriter::kInteger)
            return trap;
        trap.version = toInteger(field.value);

        if(!body.next(field) || field.tag != BerWriter::kString)
            return trap;
        trap.community = toText(field.value);

        if(!body.next(field))
            return trap;
        trap.pduTag = field.tag;

        Reader pdu(field.value.data(), static_cast<uint16_t>(field.value.size()));
        Field part;
        if(!pdu.next(part))
            return trap;
        trap.requestId = toInteger(part.value);
        if(!pdu.next(part))
            return trap;
        trap.errorStatus = toInteger(part.value);
        if(!pdu.next(part))
            return trap;
        trap.errorIndex = toInteger(part.value);

        if(!pdu.next(part) || part.tag != BerWriter::kSequence)
            return trap;

        Reader list(part.value.data(), static_cast<uint16_t>(part.value.size()));
        Field one;
        while(!list.done()) {
            if(!list.next(one) || one.tag != BerWriter::kSequence)
                return trap;
            Reader pair(one.value.data(), static_cast<uint16_t>(one.value.size()));
            Field oid;
            Bound bound;
            if(!pair.next(oid) || oid.tag != BerWriter::kOid)
                return trap;
            bound.oid = toOid(oid.value);
            if(!pair.next(bound.value))
                return trap;
            trap.bindings.push_back(bound);
        }

        trap.valid = true;
        return trap;
    }

    // --- BER: целые -------------------------------------------------------

    void test_integer_round_trip() {
        // Значения по обе стороны от границ, где меняется число байт.
        const int32_t values[] = {0,     1,      -1,      127,     128,
                                  -128,  -129,   255,     256,     32767,
                                  -32768, 65536, -65536,  2147483647L,
                                  -2147483647L - 1};
        for(int32_t value : values) {
            uint8_t buffer[32];
            BerWriter out(buffer, sizeof(buffer));
            out.putInteger(value);
            TEST_ASSERT_FALSE(out.failed());
            Reader reader(out.data(), out.size());
            Field field;
            TEST_ASSERT_TRUE(reader.next(field));
            TEST_ASSERT_EQUAL_UINT8(BerWriter::kInteger, field.tag);
            TEST_ASSERT_EQUAL_INT32(value, toInteger(field.value));
        }
    }

    // BER требует кратчайшую запись: лишние ведущие байты недопустимы.
    void test_integer_is_shortest_form() {
        struct {
            int32_t value;
            size_t bytes;
        } cases[] = {{0, 1}, {127, 1}, {128, 2}, {-128, 1}, {-129, 2},
                     {32767, 2}, {32768, 3}, {-32768, 2}, {-32769, 3}};
        for(auto& one : cases) {
            uint8_t buffer[32];
            BerWriter out(buffer, sizeof(buffer));
            out.putInteger(one.value);
            Reader reader(out.data(), out.size());
            Field field;
            TEST_ASSERT_TRUE(reader.next(field));
            TEST_ASSERT_EQUAL_size_t(one.bytes, field.value.size());
        }
    }

    // Ноль занимает один байт, а не ни одного: пустое значение недопустимо.
    void test_zero_is_one_byte() {
        uint8_t buffer[16];
        BerWriter out(buffer, sizeof(buffer));
        out.putInteger(0);
        Reader reader(out.data(), out.size());
        Field field;
        TEST_ASSERT_TRUE(reader.next(field));
        TEST_ASSERT_EQUAL_size_t(1, field.value.size());
        TEST_ASSERT_EQUAL_UINT8(0, field.value[0]);
    }

    // --- BER: строки и длины ---------------------------------------------

    void test_string_round_trip() {
        uint8_t buffer[64];
        BerWriter out(buffer, sizeof(buffer));
        out.putCString("boiler");
        Reader reader(out.data(), out.size());
        Field field;
        TEST_ASSERT_TRUE(reader.next(field));
        TEST_ASSERT_EQUAL_UINT8(BerWriter::kString, field.tag);
        TEST_ASSERT_EQUAL_STRING("boiler", toText(field.value).c_str());
    }

    // Длина до 127 пишется одним байтом, дальше - со счётчиком.
    void test_long_string_length_form() {
        std::string text(200, 'x');
        uint8_t buffer[256];
        BerWriter out(buffer, sizeof(buffer));
        out.putString(text.c_str(), static_cast<uint16_t>(text.size()));
        TEST_ASSERT_FALSE(out.failed());
        // Второй байт - признак длинной формы: один байт счётчика.
        TEST_ASSERT_EQUAL_UINT8(0x81, out.data()[1]);
        Reader reader(out.data(), out.size());
        Field field;
        TEST_ASSERT_TRUE(reader.next(field));
        TEST_ASSERT_EQUAL_size_t(200, field.value.size());
    }

    void test_string_with_zero_inside() {
        // Длина задана явно, поэтому ноль внутри - обычный байт.
        const char text[] = "ab\0cd";
        uint8_t buffer[32];
        BerWriter out(buffer, sizeof(buffer));
        out.putString(text, 5);
        Reader reader(out.data(), out.size());
        Field field;
        TEST_ASSERT_TRUE(reader.next(field));
        TEST_ASSERT_EQUAL_size_t(5, field.value.size());
        TEST_ASSERT_EQUAL_UINT8(0, field.value[2]);
    }

    // --- BER: OID ---------------------------------------------------------

    void test_oid_round_trip() {
        uint8_t buffer[64];
        BerWriter out(buffer, sizeof(buffer));
        out.putOid("1.3.6.1.4.1.12345");
        Reader reader(out.data(), out.size());
        Field field;
        TEST_ASSERT_TRUE(reader.next(field));
        TEST_ASSERT_EQUAL_UINT8(BerWriter::kOid, field.tag);
        TEST_ASSERT_EQUAL_STRING("1.3.6.1.4.1.12345", toOid(field.value).c_str());
    }

    // Первые два числа по правилам BER складываются в один байт.
    void test_oid_first_two_numbers_share_a_byte() {
        uint8_t buffer[64];
        BerWriter out(buffer, sizeof(buffer));
        out.putOid("1.3.6.1");
        Reader reader(out.data(), out.size());
        Field field;
        TEST_ASSERT_TRUE(reader.next(field));
        TEST_ASSERT_EQUAL_UINT8(1 * 40 + 3, field.value[0]);
        // 1.3.6.1 - четыре числа, но три байта.
        TEST_ASSERT_EQUAL_size_t(3, field.value.size());
    }

    // Числа больше 127 занимают несколько байт по семь бит.
    void test_oid_large_number() {
        uint8_t buffer[64];
        BerWriter out(buffer, sizeof(buffer));
        out.putOid("1.3.6.1.4.1.999999");
        Reader reader(out.data(), out.size());
        Field field;
        TEST_ASSERT_TRUE(reader.next(field));
        TEST_ASSERT_EQUAL_STRING("1.3.6.1.4.1.999999", toOid(field.value).c_str());
    }

    void test_oid_suffix_is_appended() {
        uint8_t buffer[64];
        BerWriter out(buffer, sizeof(buffer));
        out.putOid("1.3.6.1.3.1", "1.2");
        Reader reader(out.data(), out.size());
        Field field;
        TEST_ASSERT_TRUE(reader.next(field));
        TEST_ASSERT_EQUAL_STRING("1.3.6.1.3.1.1.2", toOid(field.value).c_str());
    }

    // Мусор вместо OID отмечается отказом, а не пишется как попало.
    void test_bad_oid_fails() {
        uint8_t buffer[64];
        BerWriter out(buffer, sizeof(buffer));
        out.putOid("1.3.x.1");
        TEST_ASSERT_TRUE(out.failed());
    }

    // Один номер - не OID: первые два числа кодируются вместе.
    void test_too_short_oid_fails() {
        uint8_t buffer[64];
        BerWriter out(buffer, sizeof(buffer));
        out.putOid("1");
        TEST_ASSERT_TRUE(out.failed());
    }

    // --- BER: тесный буфер ------------------------------------------------

    // Не поместилось - отказ, а не запись мимо буфера.
    void test_overflow_is_reported() {
        uint8_t buffer[4];
        BerWriter out(buffer, sizeof(buffer));
        out.putCString("much longer than four bytes");
        TEST_ASSERT_TRUE(out.failed());
        TEST_ASSERT_EQUAL_UINT16(0, out.size());
    }

    // Отказ держится: после него ничего не пишется и размер остаётся нулевым.
    void test_failure_sticks() {
        uint8_t buffer[4];
        BerWriter out(buffer, sizeof(buffer));
        out.putCString("too long for this buffer");
        TEST_ASSERT_TRUE(out.failed());
        out.putInteger(1);
        TEST_ASSERT_TRUE(out.failed());
        TEST_ASSERT_EQUAL_UINT16(0, out.size());
    }

    // --- trap -------------------------------------------------------------

    Notification makeNotification(const char* text, Level level = Level::err,
                    uint8_t source = 0) {
        Notification notification;
        notification.level = level;
        notification.source = source;
        notification.textLength = static_cast<uint16_t>(strlen(text));
        memcpy(notification.text, text, notification.textLength);
        return notification;
    }

    void test_trap_envelope() {
        uint8_t packet[FMTLOG_SNMP_PACKET_SIZE];
        TrapSettings settings;
        const Notification notification = makeNotification("sensor is silent");
        const uint16_t length =
          buildTrap(packet, sizeof(packet), notification, settings, 1234);
        TEST_ASSERT_GREATER_THAN(0, length);

        const Trap trap = parse(packet, length);
        TEST_ASSERT_TRUE(trap.valid);
        // 1 означает SNMPv2c - так задано стандартом.
        TEST_ASSERT_EQUAL_INT32(1, trap.version);
        TEST_ASSERT_EQUAL_STRING("public", trap.community.c_str());
        TEST_ASSERT_EQUAL_UINT8(BerWriter::kTrapV2, trap.pduTag);
        TEST_ASSERT_EQUAL_INT32(0, trap.errorStatus);
        TEST_ASSERT_EQUAL_INT32(0, trap.errorIndex);
    }

    // Стандарт требует, чтобы первыми шли время работы и OID события.
    void test_trap_starts_with_uptime_and_trap_oid() {
        uint8_t packet[FMTLOG_SNMP_PACKET_SIZE];
        TrapSettings settings;
        const Notification notification = makeNotification("no answer");
        const uint16_t length =
          buildTrap(packet, sizeof(packet), notification, settings, 4200);
        const Trap trap = parse(packet, length);
        TEST_ASSERT_TRUE(trap.valid);
        TEST_ASSERT_GREATER_OR_EQUAL(5, trap.bindings.size());

        TEST_ASSERT_EQUAL_STRING("1.3.6.1.2.1.1.3.0", trap.bindings[0].oid.c_str());
        TEST_ASSERT_EQUAL_UINT8(BerWriter::kTimeTicks, trap.bindings[0].value.tag);
        TEST_ASSERT_EQUAL_INT32(4200, toInteger(trap.bindings[0].value.value));

        TEST_ASSERT_EQUAL_STRING("1.3.6.1.6.3.1.1.4.1.0",
                                 trap.bindings[1].oid.c_str());
        TEST_ASSERT_EQUAL_UINT8(BerWriter::kOid, trap.bindings[1].value.tag);
    }

    void test_trap_carries_text_level_and_source() {
        uint8_t packet[FMTLOG_SNMP_PACKET_SIZE];
        TrapSettings settings;
        const Notification notification = makeNotification("pump is stuck", Level::critical, 0);
        const uint16_t length =
          buildTrap(packet, sizeof(packet), notification, settings, 1);
        const Trap trap = parse(packet, length);
        TEST_ASSERT_TRUE(trap.valid);
        TEST_ASSERT_EQUAL_size_t(5, trap.bindings.size());

        TEST_ASSERT_EQUAL_STRING("1.3.6.1.3.1.1.1", trap.bindings[2].oid.c_str());
        TEST_ASSERT_EQUAL_STRING("pump is stuck",
                                 toText(trap.bindings[2].value.value).c_str());

        TEST_ASSERT_EQUAL_STRING("1.3.6.1.3.1.1.2", trap.bindings[3].oid.c_str());
        TEST_ASSERT_EQUAL_INT32(static_cast<int32_t>(Level::critical),
                                toInteger(trap.bindings[3].value.value));

        TEST_ASSERT_EQUAL_STRING("1.3.6.1.3.1.1.3", trap.bindings[4].oid.c_str());
    }

    // Уровень входит и в OID события: по нему фильтруют, не читая текст.
    void test_trap_oid_ends_with_level() {
        uint8_t packet[FMTLOG_SNMP_PACKET_SIZE];
        TrapSettings settings;
        const Notification notification = makeNotification("gone", Level::err);
        const uint16_t length =
          buildTrap(packet, sizeof(packet), notification, settings, 1);
        const Trap trap = parse(packet, length);
        TEST_ASSERT_TRUE(trap.valid);

        const std::string expected =
          "1.3.6.1.3.1." + std::to_string(static_cast<int>(Level::err));
        TEST_ASSERT_EQUAL_STRING(expected.c_str(),
                                 toOid(trap.bindings[1].value.value).c_str());
    }

    void test_own_enterprise_oid_and_community() {
        uint8_t packet[FMTLOG_SNMP_PACKET_SIZE];
        TrapSettings settings;
        settings.setEnterpriseOid("1.3.6.1.4.1.12345");
        settings.setCommunity("secret");
        const Notification notification = makeNotification("hello");
        const uint16_t length =
          buildTrap(packet, sizeof(packet), notification, settings, 1);
        const Trap trap = parse(packet, length);
        TEST_ASSERT_TRUE(trap.valid);
        TEST_ASSERT_EQUAL_STRING("secret", trap.community.c_str());
        TEST_ASSERT_EQUAL_STRING("1.3.6.1.4.1.12345.1.1",
                                 trap.bindings[2].oid.c_str());
    }

    // Длинное сообщение перешагивает границу короткой формы длины - самое
    // место, где легко ошибиться в подсчёте.
    void test_long_message_still_parses() {
        uint8_t packet[FMTLOG_SNMP_PACKET_SIZE];
        TrapSettings settings;
        std::string text(FMTLOG_SNMP_TEXT_SIZE, 'w');
        Notification notification;
        notification.level = Level::err;
        notification.textLength = static_cast<uint16_t>(text.size());
        memcpy(notification.text, text.data(), text.size());

        const uint16_t length =
          buildTrap(packet, sizeof(packet), notification, settings, 999);
        TEST_ASSERT_GREATER_THAN(0, length);
        const Trap trap = parse(packet, length);
        TEST_ASSERT_TRUE(trap.valid);
        TEST_ASSERT_EQUAL_size_t(text.size(), trap.bindings[2].value.value.size());
    }

    void test_empty_message_is_valid() {
        uint8_t packet[FMTLOG_SNMP_PACKET_SIZE];
        TrapSettings settings;
        Notification notification;
        notification.textLength = 0;
        const uint16_t length =
          buildTrap(packet, sizeof(packet), notification, settings, 0);
        TEST_ASSERT_GREATER_THAN(0, length);
        const Trap trap = parse(packet, length);
        TEST_ASSERT_TRUE(trap.valid);
        TEST_ASSERT_EQUAL_size_t(0, trap.bindings[2].value.value.size());
    }

    // Тесный буфер - нулевая длина, а не обрезанный пакет: отправлять
    // половину trap нельзя.
    void test_small_buffer_gives_nothing() {
        uint8_t packet[32];
        TrapSettings settings;
        const Notification notification = makeNotification("this will not fit anywhere near");
        TEST_ASSERT_EQUAL_UINT16(
          0, buildTrap(packet, sizeof(packet), notification, settings, 1));
    }

    // Пакет собирается в конце буфера, а отдаётся с начала - иначе отправлять
    // пришлось бы со смещением.
    void test_packet_is_moved_to_buffer_start() {
        uint8_t packet[FMTLOG_SNMP_PACKET_SIZE];
        memset(packet, 0xAA, sizeof(packet));
        TrapSettings settings;
        const Notification notification = makeNotification("moved");
        const uint16_t length =
          buildTrap(packet, sizeof(packet), notification, settings, 1);
        TEST_ASSERT_GREATER_THAN(0, length);
        // Первый байт - метка последовательности, а не остаток заливки.
        TEST_ASSERT_EQUAL_UINT8(BerWriter::kSequence, packet[0]);
    }

} // namespace

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_integer_round_trip);
    RUN_TEST(test_integer_is_shortest_form);
    RUN_TEST(test_zero_is_one_byte);

    RUN_TEST(test_string_round_trip);
    RUN_TEST(test_long_string_length_form);
    RUN_TEST(test_string_with_zero_inside);

    RUN_TEST(test_oid_round_trip);
    RUN_TEST(test_oid_first_two_numbers_share_a_byte);
    RUN_TEST(test_oid_large_number);
    RUN_TEST(test_oid_suffix_is_appended);
    RUN_TEST(test_bad_oid_fails);
    RUN_TEST(test_too_short_oid_fails);

    RUN_TEST(test_overflow_is_reported);
    RUN_TEST(test_failure_sticks);

    RUN_TEST(test_trap_envelope);
    RUN_TEST(test_trap_starts_with_uptime_and_trap_oid);
    RUN_TEST(test_trap_carries_text_level_and_source);
    RUN_TEST(test_trap_oid_ends_with_level);
    RUN_TEST(test_own_enterprise_oid_and_community);
    RUN_TEST(test_long_message_still_parses);
    RUN_TEST(test_empty_message_is_valid);
    RUN_TEST(test_small_buffer_gives_nothing);
    RUN_TEST(test_packet_is_moved_to_buffer_start);

    return UNITY_END();
}
