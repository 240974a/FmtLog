// Тело запроса к Loki: экранирование JSON, метки и отметки времени.
//
// Проверяется разбором собранного JSON, а не сверкой строк целиком: так тест
// говорит, какое поле разошлось, и не ломается от лишнего пробела.

#include <unity.h>

#include <string>
#include <vector>

#include "FmtLokiBatch.h"

using namespace fmtlog;
using namespace fmtlog::loki;

namespace {

    // --- разбор -----------------------------------------------------------
    //
    // Читатель ровно того JSON, который пишет buildBatch: полноценный разбор
    // тесту не нужен, а сверять строки целиком - значит переписывать тест от
    // каждой запятой.

    // Значение поля "name":"..." начиная с позиции from. Возвращает как
    // записано, без снятия экранирования.
    std::string rawField(const std::string& json, const std::string& name,
                         size_t from = 0) {
        const std::string key = "\"" + name + "\":\"";
        const size_t at = json.find(key, from);
        if(at == std::string::npos)
            return "<нет>";
        size_t i = at + key.size();
        std::string out;
        while(i < json.size()) {
            if(json[i] == '\\' && i + 1 < json.size()) {
                out += json[i];
                out += json[i + 1];
                i += 2;
                continue;
            }
            if(json[i] == '"')
                break;
            out += json[i++];
        }
        return out;
    }

    // Пары ["время","строка"] из массива values.
    struct Value {
        std::string timestamp;
        std::string text;
    };

    std::vector<Value> values(const std::string& json) {
        std::vector<Value> out;
        const size_t start = json.find("\"values\":[");
        if(start == std::string::npos)
            return out;
        size_t i = start;
        while((i = json.find("[\"", i)) != std::string::npos) {
            i += 2;
            Value one;
            while(i < json.size() && json[i] != '"')
                one.timestamp += json[i++];
            i = json.find("\",\"", i);
            if(i == std::string::npos)
                break;
            i += 3;
            while(i < json.size()) {
                if(json[i] == '\\' && i + 1 < json.size()) {
                    one.text += json[i];
                    one.text += json[i + 1];
                    i += 2;
                    continue;
                }
                if(json[i] == '"')
                    break;
                one.text += json[i++];
            }
            out.push_back(one);
        }
        return out;
    }

    Entry makeEntry(const char* text, Level level = Level::info,
                    uint32_t seconds = 1757000000, uint16_t millis = 123) {
        Entry entry;
        entry.level = level;
        entry.epochSeconds = seconds;
        entry.epochMillis = millis;
        entry.textLength = static_cast<uint16_t>(strlen(text));
        memcpy(entry.text, text, entry.textLength);
        return entry;
    }

    std::string build(const Entry* entries, uint16_t count,
                      const Label* labels = nullptr, uint8_t labelCount = 0) {
        static char body[FMTLOG_LOKI_BODY_SIZE];
        const uint16_t length =
          buildBatch(body, sizeof(body), entries, count, labels, labelCount);
        return std::string(body, length);
    }

    // --- экранирование ----------------------------------------------------

    std::string escape(const char* text) {
        char out[256];
        uint16_t at = 0;
        const bool ok = writeJsonString(out, sizeof(out), at, text,
                                        static_cast<uint16_t>(strlen(text)));
        TEST_ASSERT_TRUE(ok);
        return std::string(out, at);
    }

    void test_plain_text_is_unchanged() {
        TEST_ASSERT_EQUAL_STRING("pump is stuck", escape("pump is stuck").c_str());
    }

    // Кавычка внутри строки оборвала бы JSON.
    void test_quote_is_escaped() {
        TEST_ASSERT_EQUAL_STRING("say \\\"hi\\\"", escape("say \"hi\"").c_str());
    }

    void test_backslash_is_escaped() {
        TEST_ASSERT_EQUAL_STRING("C:\\\\temp", escape("C:\\temp").c_str());
    }

    void test_newline_and_tab_are_escaped() {
        TEST_ASSERT_EQUAL_STRING("a\\nb\\tc", escape("a\nb\tc").c_str());
        TEST_ASSERT_EQUAL_STRING("a\\rb", escape("a\rb").c_str());
    }

    // Прочие управляющие знаки JSON тоже запрещает - им место \u00XX.
    void test_control_characters_use_unicode_escape() {
        TEST_ASSERT_EQUAL_STRING("a\\u0001b", escape("a\x01""b").c_str());
        TEST_ASSERT_EQUAL_STRING("\\u001f", escape("\x1f").c_str());
    }

    // UTF-8 остаётся собой: JSON его не запрещает, а плата пишет и по-русски.
    void test_utf8_passes_through() {
        TEST_ASSERT_EQUAL_STRING("насос", escape("насос").c_str());
    }

    // Не поместилось - отказ, а не обрезанная строка: половина экранирующей
    // последовательности сломала бы JSON.
    void test_escape_reports_overflow() {
        char out[4];
        uint16_t at = 0;
        TEST_ASSERT_FALSE(writeJsonString(out, sizeof(out), at, "\"\"\"\"", 4));
    }

    // --- отметки времени --------------------------------------------------

    void test_timestamp_in_nanoseconds() {
        const Entry entry = makeEntry("x", Level::info, 1757000000, 123);
        TEST_ASSERT_EQUAL_UINT64(1757000000123000000ULL, timestampNs(entry));
    }

    // Пока время не задано, ставить нечего: пусть Loki отметит сам, иначе
    // строка легла бы на 1970 год.
    void test_unknown_time_is_zero() {
        Entry entry = makeEntry("x");
        entry.epochSeconds = 0;
        entry.uptimeMs = 12340;
        TEST_ASSERT_EQUAL_UINT64(0, timestampNs(entry));
    }

    // Наносекунды не помещаются в 32 бита - обычная ошибка в таком коде.
    void test_timestamp_does_not_overflow_32_bits() {
        const Entry entry = makeEntry("x", Level::info, 4000000000U, 999);
        TEST_ASSERT_EQUAL_UINT64(4000000000999000000ULL, timestampNs(entry));
    }

    // Отметка должна занимать все девятнадцать знаков. На ПК unsigned long
    // шире, чем на плате, поэтому потерю старших разрядов там видно не по
    // значению, а по длине записи: считать её надо в uint64_t, а не в
    // unsigned long.
    void test_timestamp_keeps_full_width() {
        const Entry entry = makeEntry("x", Level::info, 1757000000, 123);
        const std::string json = build(&entry, 1);
        const auto rows = values(json);
        TEST_ASSERT_EQUAL_size_t(1, rows.size());
        TEST_ASSERT_EQUAL_size_t(19, rows[0].timestamp.size());
        // Секунды остались в старших разрядах, а не потерялись при умножении.
        TEST_ASSERT_EQUAL_STRING(
          "1757000000", rows[0].timestamp.substr(0, 10).c_str());
    }

    // --- пачка ------------------------------------------------------------

    void test_batch_structure() {
        const Entry entries[] = {makeEntry("first"), makeEntry("second")};
        const std::string json = build(entries, 2);

        TEST_ASSERT_TRUE(json.rfind("{\"streams\":[{\"stream\":{", 0) == 0);
        TEST_ASSERT_TRUE(json.size() > 4);
        TEST_ASSERT_EQUAL_STRING("]}]}", json.substr(json.size() - 4).c_str());

        const auto rows = values(json);
        TEST_ASSERT_EQUAL_size_t(2, rows.size());
        TEST_ASSERT_EQUAL_STRING("first", rows[0].text.c_str());
        TEST_ASSERT_EQUAL_STRING("second", rows[1].text.c_str());
    }

    // Уровень идёт меткой: по нему в Loki отбирают строки, не разбирая текст.
    void test_level_and_source_are_labels() {
        const Entry entries[] = {makeEntry("gone", Level::err)};
        const std::string json = build(entries, 1);
        TEST_ASSERT_EQUAL_STRING("error", rawField(json, "level").c_str());
        // Имя источника по умолчанию, пока приложение не задало свои.
        TEST_ASSERT_TRUE(rawField(json, "source") != "<нет>");
    }

    void test_application_labels_are_included() {
        Label labels[2];
        strcpy(labels[0].name, "job");
        strcpy(labels[0].value, "boiler");
        strcpy(labels[1].name, "instance");
        strcpy(labels[1].value, "kitchen");

        const Entry entries[] = {makeEntry("hello")};
        const std::string json = build(entries, 1, labels, 2);
        TEST_ASSERT_EQUAL_STRING("boiler", rawField(json, "job").c_str());
        TEST_ASSERT_EQUAL_STRING("kitchen", rawField(json, "instance").c_str());
        // Наши метки на месте вместе с чужими.
        TEST_ASSERT_EQUAL_STRING("info", rawField(json, "level").c_str());
    }

    void test_timestamp_is_written_as_string() {
        const Entry entries[] = {makeEntry("x", Level::info, 1757000000, 500)};
        const std::string json = build(entries, 1);
        const auto rows = values(json);
        TEST_ASSERT_EQUAL_size_t(1, rows.size());
        // Loki принимает наносекунды строкой: числом они не помещаются в
        // double, которым JSON считает числа.
        TEST_ASSERT_EQUAL_STRING("1757000000500000000", rows[0].timestamp.c_str());
    }

    // Кавычки в сообщении не должны развалить тело запроса.
    void test_message_with_quotes_stays_valid() {
        const Entry entries[] = {makeEntry("value is \"42\"")};
        const std::string json = build(entries, 1);
        const auto rows = values(json);
        TEST_ASSERT_EQUAL_size_t(1, rows.size());
        TEST_ASSERT_EQUAL_STRING("value is \\\"42\\\"", rows[0].text.c_str());
        // Скобки JSON на месте: строка ничего не оборвала.
        TEST_ASSERT_EQUAL_STRING("]}]}", json.substr(json.size() - 4).c_str());
    }

    void test_empty_batch_gives_nothing() {
        const Entry entries[] = {makeEntry("x")};
        char body[64];
        TEST_ASSERT_EQUAL_UINT16(0, buildBatch(body, sizeof(body), entries, 0,
                                               nullptr, 0));
    }

    // Тесный буфер - нулевая длина, а не обрезанный JSON: половину тела
    // отправлять нельзя.
    void test_small_buffer_gives_nothing() {
        const Entry entries[] = {makeEntry("a message that will not fit")};
        char body[32];
        TEST_ASSERT_EQUAL_UINT16(
          0, buildBatch(body, sizeof(body), entries, 1, nullptr, 0));
    }

    // Пустое имя метки пропускается: пустой ключ в JSON бессмыслен.
    void test_empty_label_is_skipped() {
        Label labels[2];
        labels[0].name[0] = '\0';
        strcpy(labels[1].name, "job");
        strcpy(labels[1].value, "boiler");

        const Entry entries[] = {makeEntry("x")};
        const std::string json = build(entries, 1, labels, 2);
        TEST_ASSERT_EQUAL_STRING("boiler", rawField(json, "job").c_str());
        TEST_ASSERT_EQUAL_STRING("]}]}", json.substr(json.size() - 4).c_str());
    }

    void test_full_batch_fits() {
        Entry entries[FMTLOG_LOKI_BATCH];
        for(uint16_t i = 0; i < FMTLOG_LOKI_BATCH; ++i)
            entries[i] = makeEntry("a typical log line about something");

        const std::string json = build(entries, FMTLOG_LOKI_BATCH);
        TEST_ASSERT_TRUE(json.size() > 0);
        TEST_ASSERT_EQUAL_size_t(FMTLOG_LOKI_BATCH, values(json).size());
    }

} // namespace

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_plain_text_is_unchanged);
    RUN_TEST(test_quote_is_escaped);
    RUN_TEST(test_backslash_is_escaped);
    RUN_TEST(test_newline_and_tab_are_escaped);
    RUN_TEST(test_control_characters_use_unicode_escape);
    RUN_TEST(test_utf8_passes_through);
    RUN_TEST(test_escape_reports_overflow);

    RUN_TEST(test_timestamp_in_nanoseconds);
    RUN_TEST(test_unknown_time_is_zero);
    RUN_TEST(test_timestamp_does_not_overflow_32_bits);
    RUN_TEST(test_timestamp_keeps_full_width);

    RUN_TEST(test_batch_structure);
    RUN_TEST(test_level_and_source_are_labels);
    RUN_TEST(test_application_labels_are_included);
    RUN_TEST(test_timestamp_is_written_as_string);
    RUN_TEST(test_message_with_quotes_stays_valid);
    RUN_TEST(test_empty_batch_gives_nothing);
    RUN_TEST(test_small_buffer_gives_nothing);
    RUN_TEST(test_empty_label_is_skipped);
    RUN_TEST(test_full_batch_fits);

    return UNITY_END();
}
