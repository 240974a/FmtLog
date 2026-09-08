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
        log::info("colored");
        TEST_ASSERT_TRUE(contains("\033[38;5;"));
    }

    // Без сброса цвет утёк бы на всё, что выведется в терминал дальше.
    void test_color_is_reset_at_end_of_line() {
        reset();
        log::info("colored");
        TEST_ASSERT_TRUE(contains("\033[0m"));
    }

    void test_color_can_be_disabled() {
        reset();
        color::setEnabled(false);
        log::info("plain");
        TEST_ASSERT_FALSE(contains("\033["));
        // Само сообщение при этом на месте.
        TEST_ASSERT_TRUE(contains("plain"));
    }

    void test_levels_have_distinct_colors() {
        TEST_ASSERT_NOT_EQUAL(color::levelColor(Level::info), color::levelColor(Level::err));
        TEST_ASSERT_NOT_EQUAL(color::levelColor(Level::warn), color::levelColor(Level::err));
    }

    // Иначе строка сливается в одно пятно и уровень перестаёт читаться.
    void test_no_level_matches_the_timestamp_color() {
        const Level levels[] = {Level::trace, Level::debug, Level::info, Level::warn,
                                Level::err};
        for(Level level : levels)
            TEST_ASSERT_NOT_EQUAL(color::darkGray, color::levelColor(level));
    }

    // Цвет должен быть у каждого уровня, включая critical и system: без
    // него буква сливается с цветом времени.
    void test_every_level_has_its_own_color() {
        const Level all[] = {Level::trace, Level::debug, Level::info, Level::warn,
                             Level::err, Level::critical, Level::system};
        for(size_t i = 0; i < 7; ++i) {
            TEST_ASSERT_NOT_EQUAL(color::noColor, color::levelColor(all[i]));
            TEST_ASSERT_NOT_EQUAL(color::darkGray, color::levelColor(all[i]));
        }
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


    // --- вывод в произвольный приёмник ---------------------------------
    //
    // Из этого собран и telnet: та же строка, тот же цвет.

    void test_write_goes_to_any_print() {
        reset();
        Print other;
        Record record{};
        record.level = Level::info;
        record.text = "to another stream";
        record.length = 17;
        color::write(other, record);
        TEST_ASSERT_TRUE(other.captured().find("to another stream") !=
                         std::string::npos);
        // В порт при этом ничего не ушло.
        TEST_ASSERT_EQUAL_STRING("", Serial.captured().c_str());
    }

    // Строка одна и та же, куда бы ни писали: иначе журнал в порту и в telnet
    // читался бы по-разному. Отметку времени отбрасываем - она берётся в
    // момент записи и у двух вызовов заведомо разная.
    void test_write_matches_serial_sink() {
        reset();
        log::info("same line");
        const std::string viaSerial = Serial.captured();

        Serial.reset();
        Print other;
        Record record{};
        record.level = Level::info;
        record.text = "same line";
        record.length = 9;
        color::write(other, record);

        const size_t at = viaSerial.find(" \033[38;5;");   // конец отметки
        TEST_ASSERT_TRUE(at != std::string::npos);
        TEST_ASSERT_EQUAL_STRING(viaSerial.substr(at).c_str(),
                                 other.captured().substr(at).c_str());
    }

    // Уровень, источник и текст - всё на месте, а не один текст.
    void test_write_carries_all_fields() {
        reset();
        const char* const kSources[] = {"app", "net"};
        log::setSourceNames(kSources, 2);
        Print other;
        Record record{};
        record.level = Level::err;
        record.source = 1;
        record.text = "gone";
        record.length = 4;
        color::write(other, record);
        const std::string line = other.captured();
        TEST_ASSERT_TRUE(line.find("E") != std::string::npos);
        TEST_ASSERT_TRUE(line.find("net") != std::string::npos);
        TEST_ASSERT_TRUE(line.find("gone") != std::string::npos);
    }

    // Терминал telnet без возврата каретки уводит строки лесенкой.
    void test_line_ends_with_crlf() {
        reset();
        Print other;
        Record record{};
        record.level = Level::info;
        record.text = "x";
        record.length = 1;
        color::write(other, record);
        const std::string line = other.captured();
        TEST_ASSERT_TRUE(line.size() >= 2);
        TEST_ASSERT_EQUAL_STRING("\r\n", line.substr(line.size() - 2).c_str());
    }

} // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_color_codes_appear_in_output);
    RUN_TEST(test_color_is_reset_at_end_of_line);
    RUN_TEST(test_color_can_be_disabled);
    RUN_TEST(test_levels_have_distinct_colors);
    RUN_TEST(test_no_level_matches_the_timestamp_color);
    RUN_TEST(test_every_level_has_its_own_color);
    RUN_TEST(test_source_colors_are_used);
    RUN_TEST(test_level_colors_can_be_replaced);
    RUN_TEST(test_no_color_prints_nothing);
    RUN_TEST(test_truncated_message_is_marked);
    RUN_TEST(test_write_goes_to_any_print);
    RUN_TEST(test_write_matches_serial_sink);
    RUN_TEST(test_write_carries_all_fields);
    RUN_TEST(test_line_ends_with_crlf);
    return UNITY_END();
}
