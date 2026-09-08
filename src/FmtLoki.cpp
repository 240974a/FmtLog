// Модуль собирается только там, где есть сеть. На прочих платах файл пуст:
// иначе он ломал бы сборку приложению, которое Loki и не включает - в
// библиотеке Arduino компилируются все исходники подряд.
//
// Само тело запроса сети не требует и живёт отдельно - в FmtLokiBatch.h,
// поэтому формат проверяется тестами на машине разработчика.
#if defined(ESP8266) || defined(ESP32)

#include "FmtLoki.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

#include "FmtLokiBatch.h"

namespace fmtlog {
    namespace loki {

        namespace {
            Entry queue[FMTLOG_LOKI_BATCH];
            uint16_t queued = 0;
            uint16_t lostCount = 0;

            Label labels[FMTLOG_LOKI_LABELS];
            uint8_t labelCount = 0;

            char hostName[FMTLOG_LOKI_HOST_SIZE] = {};
            char pushPath[FMTLOG_LOKI_HOST_SIZE] = "/loki/api/v1/push";
            uint16_t hostPort = 3100;
            Level threshold = Level::trace;
            bool lastSendOk = false;
            bool started = false;

            uint32_t lastSendAt = 0;

            // Отправка идёт за один проход: тело уже собрано, и ждать от Loki
            // нечего - ответ читается по возможности, а не до конца.
            //
            // Соединение каждый раз своё: держать его открытым между пачками
            // значило бы занимать сокет и следить за таймаутами Loki, а строки
            // уходят раз в несколько секунд.
            bool send(const char* body, uint16_t length) {
                WiFiClient client;
                client.setTimeout(FMTLOG_LOKI_TIMEOUT_MS);
                if(!client.connect(hostName, hostPort))
                    return false;

                char head[160];
                Fmt out(head, sizeof(head));
                out("POST ")(pushPath)(" HTTP/1.1\r\nHost: ")(hostName);
                out(":")(hostPort);
                out("\r\nContent-Type: application/json\r\nContent-Length: ");
                out(static_cast<uint32_t>(length));
                out("\r\nConnection: close\r\n\r\n");

                client.write(out.c_str(), out.length());
                client.write(body, length);

                // Ответ Loki нас не занимает: разбирать его - лишний код на
                // плате, а повторять отправку всё равно нечем.
                client.stop();
                return true;
            }

            // Сдвигает очередь на count строк.
            void dropFirst(uint16_t count) {
                if(count >= queued) {
                    queued = 0;
                    return;
                }
                for(uint16_t i = 0; i + count < queued; ++i)
                    queue[i] = queue[i + count];
                queued -= count;
            }

            // Сколько строк с начала очереди идут в один поток: у Loki строки
            // с разными метками нельзя слать вместе.
            uint16_t sameStreamRun() {
                uint16_t count = 1;
                while(count < queued && queue[count].level == queue[0].level &&
                      queue[count].source == queue[0].source)
                    ++count;
                return count;
            }

            // Собирает и отправляет одну пачку из начала очереди.
            // Возвращает false, если очередь не сдвинулась: строки остались
            // ждать. По этому признаку flush() понимает, что дальше пытаться
            // бесполезно, и не крутится на месте.
            bool sendOneBatch() {
                const uint16_t count = sameStreamRun();
                char body[FMTLOG_LOKI_BODY_SIZE];
                const uint16_t length =
                  buildBatch(body, sizeof(body), queue, count, labels, labelCount);

                if(length == 0) {
                    // Не поместилось даже в свой буфер - строку не спасти.
                    // Молчать нельзя: иначе потеря выглядела бы как отправка.
                    if(lostCount < 0xFFFF)
                        ++lostCount;
                    dropFirst(1);
                    return true;
                }

                lastSendOk = send(body, length);
                lastSendAt = millis();

                if(lastSendOk) {
                    dropFirst(count);
                    return true;
                }
                if(queued >= FMTLOG_LOKI_BATCH) {
                    // Loki недоступен, а очередь полна: место нужно новым
                    // строкам, старые теряем - и это видно в lost().
                    if(lostCount < 0xFFFF)
                        ++lostCount;
                    dropFirst(1);
                    return true;
                }
                return false;   // не ушло, строки ждут своего часа
            }
        } // namespace

        void sink(const Record& record) {
            if(record.level < threshold)
                return;

            if(queued >= FMTLOG_LOKI_BATCH) {
                // Буфер полон, а отправить сейчас нельзя - sink() зовётся
                // изнутри log::info(), сети здесь не место. Теряем самую
                // старую: свежие строки нужнее.
                if(lostCount < 0xFFFF)
                    ++lostCount;
                dropFirst(1);
            }

            Entry& into = queue[queued++];
            into.level = record.level;
            into.source = record.source;
            into.epochSeconds = record.epochSeconds;
            into.epochMillis = record.epochMillis;
            into.uptimeMs = record.uptimeMs;
            uint16_t length = static_cast<uint16_t>(record.length);
            if(length > sizeof(into.text))
                length = sizeof(into.text);
            memcpy(into.text, record.text, length);
            into.textLength = length;
        }

        void begin(const char* host, uint16_t port, const char* path) {
            strncpy(hostName, host, sizeof(hostName) - 1);
            hostName[sizeof(hostName) - 1] = '\0';
            strncpy(pushPath, path, sizeof(pushPath) - 1);
            pushPath[sizeof(pushPath) - 1] = '\0';
            hostPort = port;
            started = true;
            lastSendAt = millis();
        }

        void end() {
            started = false;
        }

        bool addLabel(const char* name, const char* value) {
            // Та же метка заново - меняем значение, а не заводим вторую:
            // одинаковые имена в JSON сделали бы поток непредсказуемым.
            for(uint8_t i = 0; i < labelCount; ++i) {
                if(strcmp(labels[i].name, name) == 0) {
                    strncpy(labels[i].value, value, FMTLOG_LOKI_LABEL_SIZE - 1);
                    labels[i].value[FMTLOG_LOKI_LABEL_SIZE - 1] = '\0';
                    return true;
                }
            }
            if(labelCount >= FMTLOG_LOKI_LABELS)
                return false;
            strncpy(labels[labelCount].name, name, FMTLOG_LOKI_LABEL_SIZE - 1);
            labels[labelCount].name[FMTLOG_LOKI_LABEL_SIZE - 1] = '\0';
            strncpy(labels[labelCount].value, value, FMTLOG_LOKI_LABEL_SIZE - 1);
            labels[labelCount].value[FMTLOG_LOKI_LABEL_SIZE - 1] = '\0';
            ++labelCount;
            return true;
        }

        void setLevel(Level level) {
            threshold = level;
        }

        uint16_t lost() {
            return lostCount;
        }

        uint16_t pending() {
            return queued;
        }

        bool connected() {
            return lastSendOk;
        }

        void flush() {
            if(!started)
                return;
            // Всё, что накопилось, - потоками, сколько бы их ни было. Но если
            // Loki недоступен, очередь не сдвинется: тогда выходим, а не
            // крутимся здесь до сторожевого таймера.
            while(queued && sendOneBatch())
                ;
        }

        void handle() {
            if(!started || queued == 0)
                return;

            // Пачка ушла, когда набралась целиком или вышло время: иначе
            // редкие строки ждали бы в буфере неизвестно сколько.
            const bool full = queued >= FMTLOG_LOKI_BATCH;
            const bool waited =
              millis() - lastSendAt >= FMTLOG_LOKI_INTERVAL_MS;
            if(!full && !waited)
                return;

            // За проход - одна пачка: loop() задерживать нельзя.
            sendOneBatch();
        }

    } // namespace loki
} // namespace fmtlog

#endif // ESP8266 || ESP32
