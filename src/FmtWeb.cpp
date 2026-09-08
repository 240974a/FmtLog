// Модуль собирается только там, где есть сеть. На прочих платах файл пуст:
// иначе он ломал бы сборку приложению, которое веб-журнал и не включает - в
// библиотеке Arduino компилируются все исходники подряд.
#if defined(ESP8266) || defined(ESP32)

#include "FmtWeb.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

namespace fmtlog {
    namespace web {

        namespace {
            History<FMTLOG_WEB_HISTORY_SIZE> history;

            WiFiServer* server = nullptr;
            WiFiClient clients[FMTLOG_WEB_CLIENTS];
            // Место каждой вкладки в истории: своё, поэтому открытая позже
            // видит ту же глубину, что и первая.
            Cursor cursors[FMTLOG_WEB_CLIENTS];

            // За раз отдаём небольшими долями: браузер может читать медленно,
            // а loop() задерживать нельзя.
            constexpr uint32_t kChunkLimit = 512;

            void dropClient(uint8_t i) {
                clients[i].stop();
                cursors[i].active = false;
            }

            // Строка потока: время, буква уровня, источник и текст, разделённые
            // табуляцией. Страница разбирает её по первому же разделителю, а в
            // самом сообщении табуляция роли не играет - оно идёт последним.
            void writeRecord(WiFiClient& to, const Record& record) {
                char head[40];
                Fmt out(head, sizeof(head));
                log::writeTimestamp(out, record);
                to.print(F("data: "));
                to.write(out.c_str(), out.length());
                to.print('\t');
                to.print(log::levelMark(record.level));
                to.print('\t');
                to.print(log::sourceName(record.source));
                to.print('\t');
                to.write(record.text, record.length);
                if(record.truncated)
                    to.print(F(" ..."));
                to.print(F("\n\n"));
            }

            void startStream(WiFiClient& to, uint8_t slot) {
                // Страница лежит у вас на диске, а поток приходит с платы -
                // для браузера это разные источники, и без этого заголовка он
                // запрос заблокирует. Плата только отдаёт журнал и ничего не
                // принимает, так что открыть его для чтения безопасно.
                to.print(F("HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/event-stream\r\n"
                           "Cache-Control: no-cache\r\n"
                           "Access-Control-Allow-Origin: *\r\n"
                           "Connection: keep-alive\r\n\r\n"));
                clients[slot] = to;
                // Новая вкладка читает с начала истории - её для того и храним.
                history.rewind(cursors[slot]);
            }

            // Досылает вкладке то, что она ещё не забрала.
            void flush(uint8_t i) {
                uint32_t sent = 0;
                while(sent < kChunkLimit) {
                    uint32_t len = 0;
                    const char* chunk = history.read(cursors[i], len);
                    if(!chunk)
                        break;
                    if(clients[i].write(chunk, len) != len) {
                        dropClient(i);
                        return;
                    }
                    sent += len;
                }
            }
        } // namespace

        void sink(const Record& record) {
            // В историю кладём уже готовую строку потока: так вкладке,
            // открытой позже, достанется ровно то же, что видели первые.
            char line[FMTLOG_MESSAGE_SIZE + 64];
            Fmt out(line, sizeof(line));
            log::writeTimestamp(out, record);
            out.write('\t');
            out.write(log::levelMark(record.level));
            out.write('\t');
            out(log::sourceName(record.source));
            out.write('\t');
            out(StringView{record.text, record.length});
            if(record.truncated)
                out(" ...");

            history.write("data: ", 6);
            history.write(out.c_str(), out.length());
            history.write("\n\n", 2);
        }

        void begin(uint16_t port) {
            end();
            server = new WiFiServer(port);
            server->begin();
            server->setNoDelay(true);
        }

        void end() {
            for(uint8_t i = 0; i < FMTLOG_WEB_CLIENTS; ++i)
                dropClient(i);
            if(server) {
                server->stop();
                delete server;
                server = nullptr;
            }
        }

        uint8_t clientCount() {
            uint8_t count = 0;
            for(uint8_t i = 0; i < FMTLOG_WEB_CLIENTS; ++i)
                if(cursors[i].active && clients[i].connected())
                    ++count;
            return count;
        }

        void handle() {
            if(!server)
                return;

            // Закрытые вкладки освобождают место сразу.
            for(uint8_t i = 0; i < FMTLOG_WEB_CLIENTS; ++i)
                if(cursors[i].active && !clients[i].connected())
                    dropClient(i);

            if(server->hasClient()) {
                WiFiClient incoming = server->accept();
                // Читаем только первую строку запроса: остальное нам не нужно,
                // а ждать всех заголовков значило бы задерживать loop().
                char request[64] = {};
                size_t at = 0;
                const uint32_t deadline = millis() + 200;
                while(incoming.connected() && millis() < deadline) {
                    if(!incoming.available())
                        continue;
                    const char c = incoming.read();
                    if(c == '\n')
                        break;
                    if(at + 1 < sizeof(request) && c != '\r')
                        request[at++] = c;
                }

                if(strstr(request, "GET /log")) {
                    uint8_t slot = FMTLOG_WEB_CLIENTS;
                    for(uint8_t i = 0; i < FMTLOG_WEB_CLIENTS; ++i)
                        if(!cursors[i].active) {
                            slot = i;
                            break;
                        }
                    if(slot < FMTLOG_WEB_CLIENTS)
                        startStream(incoming, slot);
                    else
                        incoming.stop();   // мест нет
                }
                else {
                    // Страницы здесь нет: она лежит у вас на диске. Отвечаем
                    // коротко, чтобы браузер не ждал впустую.
                    incoming.print(F("HTTP/1.1 404 Not Found\r\n"
                                     "Access-Control-Allow-Origin: *\r\n"
                                     "Content-Type: text/plain\r\n\r\n"
                                     "log stream is at /log\r\n"));
                    incoming.stop();
                }
            }

            for(uint8_t i = 0; i < FMTLOG_WEB_CLIENTS; ++i)
                if(cursors[i].active && clients[i].connected())
                    flush(i);
        }

    } // namespace web
} // namespace fmtlog

#endif // ESP8266 || ESP32
