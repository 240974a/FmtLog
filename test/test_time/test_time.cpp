// Календарное время в журнале: задание, досчёт по millis() и вывод.

#include <unity.h>

#include <string>

#include "Log.h"

using namespace fmtlog;

namespace {

    // 2026-09-03 12:30:45 UTC
    constexpr uint32_t kEpoch = 1788438645u;

    Record lastRecord{};
    std::string lastText;

    void captureSink(const Record& record) {
        lastRecord = record;
        lastText = record.text;
    }

    void reset() {
        log::clearSinks();
        log::addSink(captureSink);
        log::setLevel(Level::trace);
        log::setSourceNames(nullptr, 0);
        log::setTime(0);          // время снова неизвестно
        millisFreeze(1000);
        Serial.reset();
    }

    // --- задание времени --------------------------------------------------

    void test_time_is_unset_at_start() {
        reset();
        TEST_ASSERT_FALSE(log::timeIsSet());
    }

    void test_set_time_marks_it_known() {
        reset();
        log::setTime(kEpoch);
        TEST_ASSERT_TRUE(log::timeIsSet());
    }

    // Ноль означает "времени нет": так приложение может его и сбросить.
    void test_zero_epoch_means_unset() {
        reset();
        log::setTime(kEpoch);
        log::setTime(0);
        TEST_ASSERT_FALSE(log::timeIsSet());
    }

    void test_current_time_returns_what_was_set() {
        reset();
        log::setTime(kEpoch, 250);

        uint32_t seconds = 0;
        uint16_t millisPart = 0;
        TEST_ASSERT_TRUE(log::currentTime(seconds, millisPart));
        TEST_ASSERT_EQUAL_UINT32(kEpoch, seconds);
        TEST_ASSERT_EQUAL_UINT16(250, millisPart);
    }

    void test_current_time_fails_while_unset() {
        reset();
        uint32_t seconds = 42;
        uint16_t millisPart = 42;
        TEST_ASSERT_FALSE(log::currentTime(seconds, millisPart));
    }

    // --- досчёт по millis() -----------------------------------------------

    void test_time_advances_with_millis() {
        reset();
        log::setTime(kEpoch);
        millisAdvance(3500);

        uint32_t seconds = 0;
        uint16_t millisPart = 0;
        log::currentTime(seconds, millisPart);
        TEST_ASSERT_EQUAL_UINT32(kEpoch + 3, seconds);
        TEST_ASSERT_EQUAL_UINT16(500, millisPart);
    }

    // Доля секунды при переполнении должна поднимать секунды.
    void test_fraction_carries_into_seconds() {
        reset();
        log::setTime(kEpoch, 900);
        millisAdvance(200);

        uint32_t seconds = 0;
        uint16_t millisPart = 0;
        log::currentTime(seconds, millisPart);
        TEST_ASSERT_EQUAL_UINT32(kEpoch + 1, seconds);
        TEST_ASSERT_EQUAL_UINT16(100, millisPart);
    }

    // Повторное задание убирает накопленную погрешность хода millis().
    void test_repeated_set_time_resets_drift() {
        reset();
        log::setTime(kEpoch);
        millisAdvance(60000);      // час работы: millis() ушли вперёд

        log::setTime(kEpoch + 59); // точное время оказалось на секунду меньше

        uint32_t seconds = 0;
        uint16_t millisPart = 0;
        log::currentTime(seconds, millisPart);
        TEST_ASSERT_EQUAL_UINT32(kEpoch + 59, seconds);
        TEST_ASSERT_EQUAL_UINT16(0, millisPart);
    }

    // millis() переполняется каждые ~49.7 суток; счёт не должен сбиваться.
    void test_survives_millis_rollover() {
        reset();
        millisFreeze(0xFFFFF000u);   // до переполнения меньше секунды
        log::setTime(kEpoch);
        millisAdvance(0x2000u);      // перешли через ноль

        uint32_t seconds = 0;
        uint16_t millisPart = 0;
        log::currentTime(seconds, millisPart);
        TEST_ASSERT_EQUAL_UINT32(kEpoch + 8, seconds);
        TEST_ASSERT_EQUAL_UINT16(192, millisPart);
    }

    // --- запись журнала ---------------------------------------------------

    void test_record_carries_time() {
        reset();
        log::setTime(kEpoch, 123);
        log::info("message");
        TEST_ASSERT_EQUAL_UINT32(kEpoch, lastRecord.epochSeconds);
        TEST_ASSERT_EQUAL_UINT16(123, lastRecord.epochMillis);
    }

    // Пока времени нет, запись получает ноль - приёмник по нему и решает,
    // что показывать.
    void test_record_has_zero_time_while_unset() {
        reset();
        log::info("message");
        TEST_ASSERT_EQUAL_UINT32(0, lastRecord.epochSeconds);
    }

    // --- вывод ------------------------------------------------------------

    std::string headOf(const std::string& line, size_t width) {
        return line.substr(0, width);
    }

    void test_output_shows_calendar_time() {
        reset();
        log::clearSinks();
        log::addSink(log::serialSink);
        log::setTime(kEpoch, 123);
        log::info("message");
        TEST_ASSERT_EQUAL_STRING("26-09-03 12:30:45.123",
                                 headOf(Serial.captured(), 21).c_str());
    }

    void test_output_shows_uptime_while_unset() {
        reset();
        log::clearSinks();
        log::addSink(log::serialSink);
        millisFreeze(12340);
        log::info("message");
        TEST_ASSERT_EQUAL_STRING("steady : 00000012.340",
                                 headOf(Serial.captured(), 21).c_str());
    }

    // Ширина отметки постоянна, иначе столбцы разъезжаются на границе
    // синхронизации.
    // Ширина отметки одинакова до и после синхронизации, иначе столбцы
    // разъезжаются ровно в тот момент, когда время появляется.
    void test_timestamp_width_is_the_same_before_and_after() {
        reset();
        log::clearSinks();
        log::addSink(log::serialSink);

        millisFreeze(12340);
        log::info("a");
        const std::string uptimeLine = Serial.captured();

        Serial.reset();
        log::setTime(kEpoch, 5);
        log::info("a");
        const std::string calendarLine = Serial.captured();

        // Уровень стоит на одной и той же позиции в обеих строках.
        TEST_ASSERT_EQUAL_UINT32(calendarLine.find(" I "), uptimeLine.find(" I "));
    }

    void test_timestamp_width_is_21() {
        reset();
        log::clearSinks();
        log::addSink(log::serialSink);

        millisFreeze(12340);
        log::info("a");
        TEST_ASSERT_EQUAL_STRING("steady : 00000012.340",
                                 headOf(Serial.captured(), 21).c_str());
        TEST_ASSERT_EQUAL_CHAR(' ', Serial.captured()[21]);

        Serial.reset();
        log::setTime(kEpoch, 5);
        log::info("a");
        TEST_ASSERT_EQUAL_STRING("26-09-03 12:30:45.005",
                                 headOf(Serial.captured(), 21).c_str());
        TEST_ASSERT_EQUAL_CHAR(' ', Serial.captured()[21]);
    }

} // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_time_is_unset_at_start);
    RUN_TEST(test_set_time_marks_it_known);
    RUN_TEST(test_zero_epoch_means_unset);
    RUN_TEST(test_current_time_returns_what_was_set);
    RUN_TEST(test_current_time_fails_while_unset);
    RUN_TEST(test_time_advances_with_millis);
    RUN_TEST(test_fraction_carries_into_seconds);
    RUN_TEST(test_repeated_set_time_resets_drift);
    RUN_TEST(test_survives_millis_rollover);
    RUN_TEST(test_record_carries_time);
    RUN_TEST(test_record_has_zero_time_while_unset);
    RUN_TEST(test_output_shows_calendar_time);
    RUN_TEST(test_output_shows_uptime_while_unset);
    RUN_TEST(test_timestamp_width_is_the_same_before_and_after);
    RUN_TEST(test_timestamp_width_is_21);
    return UNITY_END();
}
