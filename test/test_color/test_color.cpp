// Проверка цветного вывода: управляющие последовательности ANSI.

#include <unity.h>

#include <string>

#include "FmtColor.h"

using namespace fmtlog;

namespace {

    void reset() {
        log::clearSinks();
        log::addSink(color::serialSink);
        log::setLevel(Level::trace);
        log::setSourceNames(nullptr, 0);
        color::setSourceColors(nullptr, 0);
        color::setLevelColors(nullptr);
        color::setEnabled(true);
        Serial.reset();
    }

    bool contains(const char* needle) {
        return Serial.captured().find(needle) != std::string::npos;
    }

    void test_color_codes_appear_in_output() {
        reset();
        log::info("цветное");
        TEST_ASSERT_TRUE(contains("\033[38;5;"));
    }

    // Без сброса цвет утёк бы на всё, что выведется в терминал дальше.
    void test_color_is_reset_at_end_of_line() {
        reset();
        log::info("цветное");
        TEST_ASSERT_TRUE(contains("\033[0m"));
    }

    void test_color_can_be_disabled() {
        reset();
        color::setEnabled(false);
        log::info("без цвета");
        TEST_ASSERT_FALSE(contains("\033["));
        // Само сообщение при этом на месте.
        TEST_ASSERT_TRUE(contains("без цвета"));
    }

    void test_levels_have_distinct_colors() {
        TEST_ASSERT_NOT_EQUAL(color::levelColor(Level::info), color::levelColor(Level::error));
        TEST_ASSERT_NOT_EQUAL(color::levelColor(Level::warning), color::levelColor(Level::error));
    }

    // Иначе строка сливается в одно пятно и уровень перестаёт читаться.
    void test_no_level_matches_the_timestamp_color() {
        const Level levels[] = {Level::trace, Level::debug, Level::info, Level::warning,
                                Level::error};
        for(Level level : levels)
            TEST_ASSERT_NOT_EQUAL(color::darkGray, color::levelColor(level));
    }

    void test_source_colors_are_used() {
        static const uint8_t colors[] = {color::green, color::cyan};
        reset();
        color::setSourceColors(colors, 2);
        TEST_ASSERT_EQUAL_UINT8(color::cyan, color::sourceColor(1));
        // Без назначенного цвета источник печатается обычным.
        TEST_ASSERT_EQUAL_UINT8(color::noColor, color::sourceColor(9));
    }

    void test_level_colors_can_be_replaced() {
        static const uint8_t custom[] = {color::white, color::white, color::magenta,
                                         color::white, color::white};
        reset();
        color::setLevelColors(custom);
        TEST_ASSERT_EQUAL_UINT8(color::magenta, color::levelColor(Level::info));
        // Пустой указатель возвращает цвета по умолчанию.
        color::setLevelColors(nullptr);
        TEST_ASSERT_NOT_EQUAL(color::magenta, color::levelColor(Level::info));
    }

    void test_no_color_prints_nothing() {
        reset();
        Serial.reset();
        color::apply(Serial, color::noColor);
        TEST_ASSERT_EQUAL_STRING("", Serial.captured().c_str());
    }

    // Обрезанное сообщение помечается, иначе потеря хвоста незаметна.
    void test_truncated_message_is_marked() {
        reset();
        std::string huge(FMTLOG_MESSAGE_SIZE * 2, 'x');
        log::info(huge.c_str());
        TEST_ASSERT_TRUE(contains("..."));
    }

} // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_color_codes_appear_in_output);
    RUN_TEST(test_color_is_reset_at_end_of_line);
    RUN_TEST(test_color_can_be_disabled);
    RUN_TEST(test_levels_have_distinct_colors);
    RUN_TEST(test_no_level_matches_the_timestamp_color);
    RUN_TEST(test_source_colors_are_used);
    RUN_TEST(test_level_colors_can_be_replaced);
    RUN_TEST(test_no_color_prints_nothing);
    RUN_TEST(test_truncated_message_is_marked);
    return UNITY_END();
}
