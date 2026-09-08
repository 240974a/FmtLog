// Кольцевой буфер с независимыми читателями.

#include <unity.h>

#include <string>

#include "FmtHistory.h"

using namespace fmtlog;

namespace {

    // Выбирает у читателя всё доступное и склеивает в строку.
    std::string drain(HistoryBase& ring, Cursor& reader) {
        std::string out;
        uint32_t len = 0;
        while(const char* chunk = ring.read(reader, len))
            out.append(chunk, len);
        return out;
    }

    void test_write_and_read() {
        History<64> ring;
        Cursor r;
        ring.rewind(r);
        ring.write("hello", 5);
        TEST_ASSERT_EQUAL_STRING("hello", drain(ring, r).c_str());
    }

    void test_nothing_to_read_twice() {
        History<64> ring;
        Cursor r;
        ring.rewind(r);
        ring.write("data", 4);
        drain(ring, r);
        // Второй заход ничего не даёт: всё уже выбрано.
        TEST_ASSERT_EQUAL_STRING("", drain(ring, r).c_str());
    }

    // Главное свойство: чтение одним не мешает другому.
    void test_two_readers_are_independent() {
        History<64> ring;
        Cursor first, second;
        ring.rewind(first);
        ring.write("message", 7);

        TEST_ASSERT_EQUAL_STRING("message", drain(ring, first).c_str());

        // Второй подключился позже - и всё равно видит историю целиком.
        ring.rewind(second);
        TEST_ASSERT_EQUAL_STRING("message", drain(ring, second).c_str());
    }

    void test_late_reader_sees_history() {
        History<64> ring;
        ring.write("first ", 6);
        ring.write("second", 6);

        Cursor late;
        ring.rewind(late);
        TEST_ASSERT_EQUAL_STRING("first second", drain(ring, late).c_str());
    }

    // Читатель, вставший на конец, старое не видит.
    void test_seek_to_end_skips_history() {
        History<64> ring;
        ring.write("old", 3);

        Cursor r;
        ring.seekToEnd(r);
        TEST_ASSERT_EQUAL_STRING("", drain(ring, r).c_str());

        ring.write("new", 3);
        TEST_ASSERT_EQUAL_STRING("new", drain(ring, r).c_str());
    }

    // --- переход через край ------------------------------------------------

    void test_wraps_around() {
        History<8> ring;
        Cursor r;
        ring.rewind(r);
        ring.write("abcdef", 6);
        TEST_ASSERT_EQUAL_STRING("abcdef", drain(ring, r).c_str());

        // Следующая запись перейдёт через край буфера.
        ring.write("ghij", 4);
        TEST_ASSERT_EQUAL_STRING("ghij", drain(ring, r).c_str());
    }

    void test_size_never_exceeds_capacity() {
        History<8> ring;
        ring.write("0123456789", 10);
        TEST_ASSERT_EQUAL_UINT32(8, ring.size());
        TEST_ASSERT_EQUAL_UINT32(8, ring.capacity());
    }

    // Кусок длиннее буфера: сохраняется его хвост, а не начало.
    void test_oversized_write_keeps_tail() {
        History<4> ring;
        Cursor r;
        ring.write("abcdefgh", 8);
        ring.rewind(r);
        TEST_ASSERT_EQUAL_STRING("efgh", drain(ring, r).c_str());
    }

    // --- отставший читатель ------------------------------------------------

    // Буфер обогнал читателя: он продолжает с самого старого и знает, сколько
    // потерял.
    void test_slow_reader_is_moved_and_marked() {
        History<8> ring;
        Cursor slow;
        ring.rewind(slow);

        ring.write("abcd", 4);      // читатель это не забрал
        ring.write("efghijkl", 8);  // и буфер его обогнал

        const std::string got = drain(ring, slow);
        TEST_ASSERT_EQUAL_STRING("efghijkl", got.c_str());
        TEST_ASSERT_EQUAL_UINT32(4, slow.lost);
    }

    void test_fast_reader_loses_nothing() {
        History<8> ring;
        Cursor fast;
        ring.rewind(fast);

        for(int i = 0; i < 10; ++i) {
            ring.write("ab", 2);
            drain(ring, fast);       // забираем сразу
        }
        TEST_ASSERT_EQUAL_UINT32(0, fast.lost);
    }

    // Отставание одного не мешает другому.
    void test_one_reader_lagging_does_not_affect_other() {
        History<8> ring;
        Cursor slow, fast;
        ring.rewind(slow);
        ring.rewind(fast);

        ring.write("abcd", 4);
        TEST_ASSERT_EQUAL_STRING("abcd", drain(ring, fast).c_str());

        ring.write("efghijkl", 8);
        drain(ring, fast);
        TEST_ASSERT_EQUAL_UINT32(0, fast.lost);

        drain(ring, slow);
        TEST_ASSERT_EQUAL_UINT32(4, slow.lost);
    }

    // Счётчик потерь шестнадцатибитный: при очень долгом отставании он
    // замирает на пределе, а не идёт по кругу, показывая малое число.
    void test_lost_counter_saturates() {
        History<8> ring;
        Cursor slow;
        ring.rewind(slow);

        // Пишем много и ни разу не читаем.
        for(int i = 0; i < 10000; ++i)
            ring.write("abcdefgh", 8);

        ring.pending(slow);   // здесь читатель подтягивается вперёд
        TEST_ASSERT_EQUAL_UINT16(0xFFFF, slow.lost);
    }

    // Размер курсора важен: их по одному на соединение.
    void test_cursor_is_small() {
        TEST_ASSERT_TRUE(sizeof(Cursor) <= 8);
    }

    // --- счётчики ----------------------------------------------------------

    void test_pending_counts_unread() {
        History<64> ring;
        Cursor r;
        ring.rewind(r);
        ring.write("12345", 5);
        TEST_ASSERT_EQUAL_UINT32(5, ring.pending(r));
        drain(ring, r);
        TEST_ASSERT_EQUAL_UINT32(0, ring.pending(r));
    }

    void test_inactive_reader_reads_nothing() {
        History<64> ring;
        Cursor r;              // не вставлен - active == false
        ring.write("data", 4);
        TEST_ASSERT_EQUAL_UINT32(0, ring.pending(r));
        TEST_ASSERT_EQUAL_STRING("", drain(ring, r).c_str());
    }

    void test_written_counts_everything() {
        History<4> ring;
        ring.write("abcdef", 6);
        TEST_ASSERT_EQUAL_UINT32(6, ring.written());
    }

    // --- вырожденные случаи -------------------------------------------------

    void test_empty_and_null_writes() {
        History<8> ring;
        Cursor r;
        ring.rewind(r);
        ring.write(nullptr, 5);
        ring.write("x", 0);
        TEST_ASSERT_EQUAL_UINT32(0, ring.written());
        TEST_ASSERT_EQUAL_STRING("", drain(ring, r).c_str());
    }

    void test_single_char_write() {
        History<8> ring;
        Cursor r;
        ring.rewind(r);
        ring.write('x');
        TEST_ASSERT_EQUAL_STRING("x", drain(ring, r).c_str());
    }

} // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_write_and_read);
    RUN_TEST(test_nothing_to_read_twice);
    RUN_TEST(test_two_readers_are_independent);
    RUN_TEST(test_late_reader_sees_history);
    RUN_TEST(test_seek_to_end_skips_history);
    RUN_TEST(test_wraps_around);
    RUN_TEST(test_size_never_exceeds_capacity);
    RUN_TEST(test_oversized_write_keeps_tail);
    RUN_TEST(test_slow_reader_is_moved_and_marked);
    RUN_TEST(test_fast_reader_loses_nothing);
    RUN_TEST(test_one_reader_lagging_does_not_affect_other);
    RUN_TEST(test_lost_counter_saturates);
    RUN_TEST(test_cursor_is_small);
    RUN_TEST(test_pending_counts_unread);
    RUN_TEST(test_inactive_reader_reads_nothing);
    RUN_TEST(test_written_counts_everything);
    RUN_TEST(test_empty_and_null_writes);
    RUN_TEST(test_single_char_write);
    return UNITY_END();
}
