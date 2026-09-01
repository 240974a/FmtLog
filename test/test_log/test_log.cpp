// Проверка журнала: уровни, источники, приёмники.
//
// Раскраска вынесена в отдельный модуль, её проверяет test_color.

#include <unity.h>

#include <string>

#include "Log.h"

using namespace fmtlog;

namespace {

    // Приёмник, запоминающий последнюю запись: так видно, что дошло до вывода.
    Record lastRecord{};
    std::string lastText;
    int recordCount = 0;

    void captureSink(const Record& record) {
        lastRecord = record;
        lastText = record.text;
        ++recordCount;
    }

    void reset() {
        log::clearSinks();
        log::addSink(captureSink);
        log::setLevel(Level::trace);
        log::setSourceNames(nullptr, 0);
        lastText.clear();
        recordCount = 0;
        Serial.reset();
    }

    // --- уровни -----------------------------------------------------------

    void test_message_reaches_sink() {
        reset();
        log::info("пин {}", 13);
        TEST_ASSERT_EQUAL_INT(1, recordCount);
        TEST_ASSERT_EQUAL_STRING("пин 13", lastText.c_str());
    }

    // Сообщение ниже порога не должно собираться вовсе.
    void test_level_below_threshold_is_dropped() {
        reset();
        log::setLevel(Level::warning);
        log::info("не должно появиться");
        TEST_ASSERT_EQUAL_INT(0, recordCount);
    }

    void test_level_at_threshold_passes() {
        reset();
        log::setLevel(Level::warning);
        log::warning("порог");
        log::error("выше порога");
        TEST_ASSERT_EQUAL_INT(2, recordCount);
    }

    void test_level_none_silences_everything() {
        reset();
        log::setLevel(Level::none);
        log::error("даже ошибка молчит");
        TEST_ASSERT_EQUAL_INT(0, recordCount);
    }

    // Уровень источника перекрывает общий - только для своего источника.
    void test_source_level_overrides_common() {
        reset();
        log::setLevel(Level::error);
        log::setLevel(2, Level::debug);

        log::debugFrom(1, "приглушён общим уровнем");
        TEST_ASSERT_EQUAL_INT(0, recordCount);

        log::debugFrom(2, "у источника свой уровень");
        TEST_ASSERT_EQUAL_INT(1, recordCount);
    }

    void test_enabled_matches_actual_output() {
        reset();
        log::setLevel(Level::info);
        TEST_ASSERT_FALSE(log::enabled(Level::debug));
        TEST_ASSERT_TRUE(log::enabled(Level::info));
        TEST_ASSERT_TRUE(log::enabled(Level::error));
    }

    // --- приёмники --------------------------------------------------------

    void test_several_sinks_receive_the_same_record() {
        reset();
        static int second = 0;
        second = 0;
        log::addSink([](const Record&) { ++second; });
        log::info("одно сообщение");
        TEST_ASSERT_EQUAL_INT(1, recordCount);
        TEST_ASSERT_EQUAL_INT(1, second);
    }

    void test_sink_is_not_added_twice() {
        reset();
        log::addSink(captureSink); // тот же самый
        log::info("одно сообщение");
        TEST_ASSERT_EQUAL_INT(1, recordCount);
    }

    void test_removed_sink_stops_receiving() {
        reset();
        log::removeSink(captureSink);
        log::info("некуда выводить");
        TEST_ASSERT_EQUAL_INT(0, recordCount);
    }

    void test_empty_message_is_not_dispatched() {
        reset();
        log::info("");
        TEST_ASSERT_EQUAL_INT(0, recordCount);
    }

    // --- источники --------------------------------------------------------

    void test_source_reaches_record() {
        reset();
        log::infoFrom(3, "от третьего");
        TEST_ASSERT_EQUAL_UINT8(3, lastRecord.source);
    }

    void test_source_name_falls_back_to_number() {
        reset();
        TEST_ASSERT_EQUAL_STRING("5", log::sourceName(5));
    }

    void test_source_names_are_used() {
        static const char* const names[] = {"app", "net"};
        reset();
        log::setSourceNames(names, 2);
        TEST_ASSERT_EQUAL_STRING("net", log::sourceName(1));
        // За пределами списка снова номер.
        TEST_ASSERT_EQUAL_STRING("7", log::sourceName(7));
    }

    // --- обрезка ----------------------------------------------------------

    // Сообщение длиннее буфера должно доходить обрезанным и помеченным.
    void test_long_message_is_marked_truncated() {
        reset();
        std::string huge(FMTLOG_MESSAGE_SIZE * 2, 'x');
        log::info(huge.c_str());
        TEST_ASSERT_EQUAL_INT(1, recordCount);
        TEST_ASSERT_TRUE(lastRecord.truncated);
        TEST_ASSERT_TRUE(lastText.length() < huge.length());
    }

} // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_message_reaches_sink);
    RUN_TEST(test_level_below_threshold_is_dropped);
    RUN_TEST(test_level_at_threshold_passes);
    RUN_TEST(test_level_none_silences_everything);
    RUN_TEST(test_source_level_overrides_common);
    RUN_TEST(test_enabled_matches_actual_output);
    RUN_TEST(test_several_sinks_receive_the_same_record);
    RUN_TEST(test_sink_is_not_added_twice);
    RUN_TEST(test_removed_sink_stops_receiving);
    RUN_TEST(test_empty_message_is_not_dispatched);
    RUN_TEST(test_source_reaches_record);
    RUN_TEST(test_source_name_falls_back_to_number);
    RUN_TEST(test_source_names_are_used);
    RUN_TEST(test_long_message_is_marked_truncated);
    return UNITY_END();
}
