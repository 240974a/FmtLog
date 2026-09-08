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
        log::info("pin {}", 13);
        TEST_ASSERT_EQUAL_INT(1, recordCount);
        TEST_ASSERT_EQUAL_STRING("pin 13", lastText.c_str());
    }

    // Сообщение ниже порога не должно собираться вовсе.
    void test_level_below_threshold_is_dropped() {
        reset();
        log::setLevel(Level::warn);
        log::info("must not appear");
        TEST_ASSERT_EQUAL_INT(0, recordCount);
    }

    void test_level_at_threshold_passes() {
        reset();
        log::setLevel(Level::warn);
        log::warn("at threshold");
        log::err("above threshold");
        TEST_ASSERT_EQUAL_INT(2, recordCount);
    }

    void test_level_none_silences_everything() {
        reset();
        log::setLevel(Level::none);
        log::err("even errors are silent");
        TEST_ASSERT_EQUAL_INT(0, recordCount);
    }

    // Уровень источника перекрывает общий - только для своего источника.
    void test_source_level_overrides_common() {
        reset();
        log::setLevel(Level::err);
        log::setLevel(2, Level::debug);

        log::debugFrom(1, "muted by the common level");
        TEST_ASSERT_EQUAL_INT(0, recordCount);

        log::debugFrom(2, "source has its own level");
        TEST_ASSERT_EQUAL_INT(1, recordCount);
    }

    void test_enabled_matches_actual_output() {
        reset();
        log::setLevel(Level::info);
        TEST_ASSERT_FALSE(log::enabled(Level::debug));
        TEST_ASSERT_TRUE(log::enabled(Level::info));
        TEST_ASSERT_TRUE(log::enabled(Level::err));
    }

    // Critical и system нужны в журнале всегда - порог их не глушит.
    void test_mandatory_levels_ignore_threshold() {
        reset();
        log::setLevel(Level::none);

        log::info("dropped");
        TEST_ASSERT_EQUAL_INT(0, recordCount);

        log::critical("always shown");
        TEST_ASSERT_EQUAL_INT(1, recordCount);

        log::system("also always");
        TEST_ASSERT_EQUAL_INT(2, recordCount);
    }

    void test_mandatory_levels_ignore_source_threshold() {
        reset();
        log::setLevel(2, Level::none);
        log::criticalFrom(2, "still shown");
        TEST_ASSERT_EQUAL_INT(1, recordCount);
    }

    void test_enabled_reports_mandatory_as_enabled() {
        reset();
        log::setLevel(Level::none);
        TEST_ASSERT_FALSE(log::enabled(Level::err));
        TEST_ASSERT_TRUE(log::enabled(Level::critical));
        TEST_ASSERT_TRUE(log::enabled(Level::system));
    }

    void test_level_marks_are_distinct() {
        const Level all[] = {Level::trace, Level::debug, Level::info, Level::warn,
                             Level::err, Level::critical, Level::system};
        for(size_t i = 0; i < 7; ++i)
            for(size_t j = i + 1; j < 7; ++j)
                TEST_ASSERT_NOT_EQUAL(log::levelMark(all[i]), log::levelMark(all[j]));
    }

    // --- уровни от приложения ----------------------------------------------

    Level externalLevels[4] = {Level::info, Level::info, Level::info, Level::info};

    Level askApplication(uint8_t source) {
        return source < 4 ? externalLevels[source] : Level::info;
    }

    // Когда уровни хранит приложение, правка действует сразу - библиотеке
    // ничего сообщать не нужно.
    void test_level_source_is_asked_every_time() {
        reset();
        externalLevels[1] = Level::err;
        log::setLevelSource(askApplication);

        log::infoFrom(1, "dropped");
        TEST_ASSERT_EQUAL_INT(0, recordCount);

        externalLevels[1] = Level::trace;   // приложение изменило уровень
        log::infoFrom(1, "now shown");
        TEST_ASSERT_EQUAL_INT(1, recordCount);

        log::setLevelSource(nullptr);
    }

    // Пока источник задан, setLevel библиотеки на решение не влияет.
    void test_level_source_overrides_stored_levels() {
        reset();
        externalLevels[0] = Level::err;
        log::setLevelSource(askApplication);
        log::setLevel(Level::trace);        // библиотеке это не поможет

        log::info("dropped");
        TEST_ASSERT_EQUAL_INT(0, recordCount);

        log::setLevelSource(nullptr);
    }

    // --- приёмники --------------------------------------------------------

    void test_several_sinks_receive_the_same_record() {
        reset();
        static int second = 0;
        second = 0;
        log::addSink([](const Record&) { ++second; });
        log::info("one message");
        TEST_ASSERT_EQUAL_INT(1, recordCount);
        TEST_ASSERT_EQUAL_INT(1, second);
    }

    void test_sink_is_not_added_twice() {
        reset();
        log::addSink(captureSink); // тот же самый
        log::info("one message");
        TEST_ASSERT_EQUAL_INT(1, recordCount);
    }

    void test_removed_sink_stops_receiving() {
        reset();
        log::removeSink(captureSink);
        log::info("nowhere to write");
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
        log::infoFrom(3, "from the third");
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
    RUN_TEST(test_mandatory_levels_ignore_threshold);
    RUN_TEST(test_mandatory_levels_ignore_source_threshold);
    RUN_TEST(test_enabled_reports_mandatory_as_enabled);
    RUN_TEST(test_level_marks_are_distinct);
    RUN_TEST(test_level_source_is_asked_every_time);
    RUN_TEST(test_level_source_overrides_stored_levels);
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
