// Проверка форматирования по образцу.
//
// Тесты идут на машине разработчика: `pio test -e native`. Плата не нужна -
// разбор образца и правила вывода от неё не зависят.

#include <unity.h>

#include <math.h>
#include <string.h>

#include "Fmt.h"

using namespace fmtlog;

namespace {

    // --- разбор образца ---------------------------------------------------

    void test_substitutes_in_order() {
        char buf[64];
        Fmt out(buf, sizeof(buf));
        out.format("пин {} = {}", 13, 1);
        TEST_ASSERT_EQUAL_STRING("пин 13 = 1", out.c_str());
    }

    void test_pattern_without_placeholders() {
        char buf[64];
        Fmt out(buf, sizeof(buf));
        out.format("без вставок");
        TEST_ASSERT_EQUAL_STRING("без вставок", out.c_str());
    }

    // Значений меньше, чем мест: лишние {} не должны остаться в сообщении.
    void test_missing_arguments_leave_nothing() {
        char buf[64];
        Fmt out(buf, sizeof(buf));
        out.format("{} и {}", 1);
        TEST_ASSERT_EQUAL_STRING("1 и ", out.c_str());
    }

    // Значений больше, чем мест: лишние молча отбрасываются.
    void test_extra_arguments_are_ignored() {
        char buf[64];
        Fmt out(buf, sizeof(buf));
        out.format("только {}", 1, 2, 3);
        TEST_ASSERT_EQUAL_STRING("только 1", out.c_str());
    }

    // Иначе фигурную скобку нельзя было бы вывести вовсе.
    void test_doubled_braces_collapse() {
        char buf[64];
        Fmt out(buf, sizeof(buf));
        out.format("{{не вставка}}", 1);
        TEST_ASSERT_EQUAL_STRING("{не вставка}", out.c_str());
    }

    void test_lone_brace_is_printed() {
        char buf[64];
        Fmt out(buf, sizeof(buf));
        out.format("{ одинокая", 1);
        TEST_ASSERT_EQUAL_STRING("{ одинокая", out.c_str());
    }

    void test_empty_and_null_pattern() {
        char buf[64];
        Fmt a(buf, sizeof(buf));
        a.format("");
        TEST_ASSERT_EQUAL_STRING("", a.c_str());

        char buf2[64];
        Fmt b(buf2, sizeof(buf2));
        b.format(static_cast<const char*>(nullptr));
        TEST_ASSERT_EQUAL_STRING("", b.c_str());
    }

    // --- граница буфера ---------------------------------------------------

    void test_overflow_truncates() {
        char buf[11]; // 10 символов плюс завершающий ноль
        Fmt out(buf, sizeof(buf));
        out.format("1234567890ABCDEF");
        TEST_ASSERT_EQUAL_STRING("1234567890", out.c_str());
        TEST_ASSERT_TRUE(out.truncated());
    }

    void test_overflow_truncates_mid_value() {
        char buf[9];
        Fmt out(buf, sizeof(buf));
        out.format("12345{}", 999999);
        TEST_ASSERT_EQUAL_STRING("12345999", out.c_str());
        TEST_ASSERT_TRUE(out.truncated());
    }

    void test_not_truncated_when_it_fits() {
        char buf[16];
        Fmt out(buf, sizeof(buf));
        out.format("{}", 42);
        TEST_ASSERT_FALSE(out.truncated());
    }

    // --- числа ------------------------------------------------------------

    void test_negative_numbers() {
        char buf[32];
        Fmt out(buf, sizeof(buf));
        out.format("{} {}", -42, -1);
        TEST_ASSERT_EQUAL_STRING("-42 -1", out.c_str());
    }

    // Минимальное значение по модулю не представимо в знаковом типе:
    // прямая смена знака здесь переполняется.
    void test_int32_min() {
        char buf[32];
        Fmt out(buf, sizeof(buf));
        out.format("{}", static_cast<int32_t>(-2147483647 - 1));
        TEST_ASSERT_EQUAL_STRING("-2147483648", out.c_str());
    }

    void test_unsigned_upper_bound() {
        char buf[32];
        Fmt out(buf, sizeof(buf));
        out.format("{}", static_cast<uint32_t>(4294967295u));
        TEST_ASSERT_EQUAL_STRING("4294967295", out.c_str());
    }

    void test_zero() {
        char buf[16];
        Fmt out(buf, sizeof(buf));
        out.format("{}", 0);
        TEST_ASSERT_EQUAL_STRING("0", out.c_str());
    }

    void test_bool_and_char() {
        char buf[32];
        Fmt out(buf, sizeof(buf));
        out.format("{} {} {}", true, false, 'x');
        TEST_ASSERT_EQUAL_STRING("true false x", out.c_str());
    }

    // --- длительность -----------------------------------------------------

    void test_duration() {
        char buf[32];
        Fmt a(buf, sizeof(buf));
        a.format("{}", Duration(1250));
        TEST_ASSERT_EQUAL_STRING("1s:250ms", a.c_str());

        char buf2[32];
        Fmt b(buf2, sizeof(buf2));
        b.format("{}", Duration(90));
        TEST_ASSERT_EQUAL_STRING("90ms", b.c_str());

        char buf3[32];
        Fmt c(buf3, sizeof(buf3));
        c.format("{}", Duration(12000));
        TEST_ASSERT_EQUAL_STRING("12s", c.c_str());
    }

    // --- дата и время -----------------------------------------------------

    void test_datetime() {
        char buf[32];
        Fmt out(buf, sizeof(buf));
        // 2024-02-29 12:34:56 UTC - високосный год
        out.format("{}", DateTime(1709210096u));
        TEST_ASSERT_EQUAL_STRING("29.02.2024 12:34:56", out.c_str());
    }

    void test_time_of_day() {
        char buf[32];
        Fmt out(buf, sizeof(buf));
        out.format("{}", TimeOfDay(1709210096u));
        TEST_ASSERT_EQUAL_STRING("12:34:56", out.c_str());
    }

    void test_epoch_start() {
        char buf[32];
        Fmt out(buf, sizeof(buf));
        out.format("{}", DateTime(0));
        TEST_ASSERT_EQUAL_STRING("01.01.1970 00:00:00", out.c_str());
    }

    // --- дампы ------------------------------------------------------------

    void test_hex_dump() {
        const uint8_t data[] = {0x0A, 0xFF, 0x00, 0x12};
        char buf[32];
        Fmt out(buf, sizeof(buf));
        out.format("{}", HexDump(data, sizeof(data)));
        TEST_ASSERT_EQUAL_STRING("0A,FF,00,12", out.c_str());
    }

    void test_full_dump_shows_text_and_bytes() {
        const uint8_t data[] = {'O', 'k', 0x01};
        char buf[64];
        Fmt out(buf, sizeof(buf));
        out.format("{}", FullDump(data, sizeof(data)));
        TEST_ASSERT_EQUAL_STRING("\"Ok.\" 0x4F 6B 01", out.c_str());
    }

    // --- необязательное значение ------------------------------------------

    void test_opt_absent_prints_nothing() {
        char buf[64];
        Fmt out(buf, sizeof(buf));
        out.format("команда:{}", opt(false, " = ", 42));
        TEST_ASSERT_EQUAL_STRING("команда:", out.c_str());
    }

    void test_opt_present_prints_prefix_and_value() {
        char buf[64];
        Fmt out(buf, sizeof(buf));
        out.format("команда:{}", opt(true, " = ", 42));
        TEST_ASSERT_EQUAL_STRING("команда: = 42", out.c_str());
    }

    void test_opt_with_suffix() {
        char buf[64];
        Fmt out(buf, sizeof(buf));
        out.format("{}конец", opt(true, "(", 42, ") "));
        TEST_ASSERT_EQUAL_STRING("(42) конец", out.c_str());
    }

    // Место вставки остаётся занятым и при ложном условии: следующее значение
    // не должно въехать на его место.
    void test_opt_absent_keeps_argument_order() {
        char buf[64];
        Fmt out(buf, sizeof(buf));
        out.format("a{}b{}c", opt(false, "X"), "Y");
        TEST_ASSERT_EQUAL_STRING("abYc", out.c_str());
    }

    // --- свой тип ---------------------------------------------------------

    struct Point {
        int x, y;
    };

    void test_custom_type() {
        char buf[32];
        Fmt out(buf, sizeof(buf));
        out.format("точка {}", Point{3, 4});
        TEST_ASSERT_EQUAL_STRING("точка (3, 4)", out.c_str());
    }

    // --- дописывание ------------------------------------------------------

    void test_append_without_pattern() {
        char buf[32];
        Fmt out(buf, sizeof(buf));
        out("a")(1)('-')(true);
        TEST_ASSERT_EQUAL_STRING("a1-true", out.c_str());
    }

    void test_clear_resets() {
        char buf[32];
        Fmt out(buf, sizeof(buf));
        out.format("первое");
        out.clear();
        out.format("второе");
        TEST_ASSERT_EQUAL_STRING("второе", out.c_str());
    }


    // --- дробные ----------------------------------------------------------

    void test_float_basic() {
        char buf[32];
        Fmt out(buf, sizeof(buf));
        out.format("{}", 54.25);
        TEST_ASSERT_EQUAL_STRING("54.250", out.c_str());
    }

    void test_float_negative() {
        char buf[32];
        Fmt out(buf, sizeof(buf));
        out.format("{} {}", -1.5, -0.001);
        TEST_ASSERT_EQUAL_STRING("-1.500 -0.001", out.c_str());
    }

    // У -0.0 сравнение с нулём ложно, и минус легко потерять.
    void test_float_negative_zero_keeps_sign() {
        char buf[32];
        Fmt out(buf, sizeof(buf));
        out.format("{}", -0.0);
        TEST_ASSERT_EQUAL_STRING("-0.000", out.c_str());
    }

    // Без ведущих нулей 0.05 напечаталось бы как 0.5
    void test_float_leading_zeros_in_fraction() {
        char buf[32];
        Fmt out(buf, sizeof(buf));
        out.format("{} {} {}", 0.05, 0.005, 0.5);
        TEST_ASSERT_EQUAL_STRING("0.050 0.005 0.500", out.c_str());
    }

    // Округление могло переполнить дробную часть и должно поднять целую.
    void test_float_rounding_carries_into_whole() {
        char buf[32];
        Fmt out(buf, sizeof(buf));
        out.format("{}", 0.9999);
        TEST_ASSERT_EQUAL_STRING("1.000", out.c_str());
    }

    // Ровная половина округляется к чётному - как в printf и dtostrf.
    void test_float_half_rounds_to_even() {
        char buf[64];
        Fmt out(buf, sizeof(buf));
        formatFloat(out, 0.5, 0);
        out.write(' ');
        formatFloat(out, 1.5, 0);
        out.write(' ');
        formatFloat(out, 2.5, 0);
        out.write(' ');
        formatFloat(out, 3.5, 0);
        TEST_ASSERT_EQUAL_STRING("0 2 2 4", out.c_str());
    }

    void test_float_zero_decimals() {
        char buf[32];
        Fmt out(buf, sizeof(buf));
        formatFloat(out, 3.7, 0);
        TEST_ASSERT_EQUAL_STRING("4", out.c_str());
    }

    // За пределом 2^32 дробной части у double уже нет.
    void test_float_large_value() {
        char buf[48];
        Fmt out(buf, sizeof(buf));
        formatFloat(out, 4294967296.0, 2);
        TEST_ASSERT_EQUAL_STRING("4294967296.00", out.c_str());
    }

    void test_float_nan_and_inf() {
        char buf[32];
        Fmt out(buf, sizeof(buf));
        out.format("{} {} {}", NAN, INFINITY, -INFINITY);
        TEST_ASSERT_EQUAL_STRING("nan inf -inf", out.c_str());
    }

} // namespace

// Так подключается свой тип: одна специализация formatter<T>.
template<>
struct fmtlog::formatter<Point> {
    static void format(Fmt& out, const Point& value) {
        out.format("({}, {})", value.x, value.y);
    }
};

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_substitutes_in_order);
    RUN_TEST(test_pattern_without_placeholders);
    RUN_TEST(test_missing_arguments_leave_nothing);
    RUN_TEST(test_extra_arguments_are_ignored);
    RUN_TEST(test_doubled_braces_collapse);
    RUN_TEST(test_lone_brace_is_printed);
    RUN_TEST(test_empty_and_null_pattern);
    RUN_TEST(test_overflow_truncates);
    RUN_TEST(test_overflow_truncates_mid_value);
    RUN_TEST(test_not_truncated_when_it_fits);
    RUN_TEST(test_negative_numbers);
    RUN_TEST(test_int32_min);
    RUN_TEST(test_unsigned_upper_bound);
    RUN_TEST(test_zero);
    RUN_TEST(test_bool_and_char);
    RUN_TEST(test_float_basic);
    RUN_TEST(test_float_negative);
    RUN_TEST(test_float_negative_zero_keeps_sign);
    RUN_TEST(test_float_leading_zeros_in_fraction);
    RUN_TEST(test_float_rounding_carries_into_whole);
    RUN_TEST(test_float_half_rounds_to_even);
    RUN_TEST(test_float_zero_decimals);
    RUN_TEST(test_float_large_value);
    RUN_TEST(test_float_nan_and_inf);
    RUN_TEST(test_duration);
    RUN_TEST(test_datetime);
    RUN_TEST(test_time_of_day);
    RUN_TEST(test_epoch_start);
    RUN_TEST(test_hex_dump);
    RUN_TEST(test_full_dump_shows_text_and_bytes);
    RUN_TEST(test_opt_absent_prints_nothing);
    RUN_TEST(test_opt_present_prints_prefix_and_value);
    RUN_TEST(test_opt_with_suffix);
    RUN_TEST(test_opt_absent_keeps_argument_order);
    RUN_TEST(test_custom_type);
    RUN_TEST(test_append_without_pattern);
    RUN_TEST(test_clear_resets);
    return UNITY_END();
}
