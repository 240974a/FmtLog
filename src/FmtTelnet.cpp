// Модуль собирается только там, где есть сеть. На прочих платах файл пуст:
// иначе он ломал бы сборку приложению, которое telnet и не включает - в
// библиотеке Arduino компилируются все исходники подряд.
#if defined(ESP8266) || defined(ESP32)

#include "FmtTelnet.h"
#include "FmtColor.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

namespace fmtlog {
    namespace telnet {

        namespace {
            History<FMTLOG_HISTORY_SIZE> history;

            WiFiServer* server = nullptr;
            WiFiClient clients[FMTLOG_TELNET_CLIENTS];
            // Место каждого клиента в истории: своё, поэтому чтение одним
            // ничего не забирает у остальных.
            Cursor cursors[FMTLOG_TELNET_CLIENTS];

            bool sendHistory = true;

            // За раз отдаём небольшими долями: клиент может читать медленно,
            // а loop() задерживать нельзя.
            constexpr uint32_t kChunkLimit = 512;

            void dropClient(uint8_t i) {
                clients[i].stop();
                cursors[i].active = false;
            }

            // Сообщает клиенту, что часть истории до него не дошла.
            void reportLoss(uint8_t i) {
                char note[48];
                Fmt out(note, sizeof(note));
                out.format(F("\r\n[{} bytes lost]\r\n"), cursors[i].lost);
                clients[i].write(out.c_str(), out.length());
                cursors[i].lost = 0;
            }

            // Досылает клиенту то, что он ещё не забрал.
            void flush(uint8_t i) {
                uint32_t sent = 0;
                while(sent < kChunkLimit) {
                    uint32_t len = 0;
                    const char* chunk = history.read(cursors[i], len);
                    if(!chunk)
                        break;
                    if(clients[i].write(chunk, len) != len) {
                        // Клиент не принял - соединение потеряно.
                        dropClient(i);
                        return;
                    }
                    sent += len;
                }
                // О потере говорим после отправки: иначе отметка встала бы
                // посреди недосланного куска.
                if(cursors[i].lost)
                    reportLoss(i);
            }
        } // namespace

        namespace {
            // Пишет строку прямо в историю: собирать её во временный буфер, а
            // потом копировать - лишний расход стека на каждое сообщение.
            class ToHistory : public Print {
              public:
                size_t write(uint8_t byte) override {
                    history.write(static_cast<char>(byte));
                    return 1;
                }
                size_t write(const uint8_t* data, size_t length) override {
                    history.write(reinterpret_cast<const char*>(data),
                                  static_cast<uint32_t>(length));
                    return length;
                }
            };
        } // namespace

        void sink(const Record& record) {
            // Та же строка, что уходит в порт: время, уровень, источник и
            // текст. Цвет тоже - терминал понимает те же последовательности
            // ANSI, а кому он мешает, тот снимет его color::setEnabled(false).
            ToHistory out;
            color::write(out, record);
        }

        void begin(uint16_t port) {
            end();
            server = new WiFiServer(port);
            server->begin();
            server->setNoDelay(true);
        }

        void end() {
            for(uint8_t i = 0; i < FMTLOG_TELNET_CLIENTS; ++i)
                dropClient(i);
            if(server) {
                server->stop();
                delete server;
                server = nullptr;
            }
        }

        void setSendHistory(bool send) {
            sendHistory = send;
        }

        uint8_t clientCount() {
            uint8_t count = 0;
            for(uint8_t i = 0; i < FMTLOG_TELNET_CLIENTS; ++i)
                if(clients[i] && clients[i].connected())
                    ++count;
            return count;
        }

        void handle() {
            if(!server)
                return;

            // Отвалившиеся соединения освобождают место сразу, а не при
            // следующей попытке подключения.
            for(uint8_t i = 0; i < FMTLOG_TELNET_CLIENTS; ++i)
                if(cursors[i].active && !clients[i].connected())
                    dropClient(i);

            if(server->hasClient()) {
                uint8_t slot = FMTLOG_TELNET_CLIENTS;
                for(uint8_t i = 0; i < FMTLOG_TELNET_CLIENTS; ++i)
                    if(!cursors[i].active) {
                        slot = i;
                        break;
                    }

                if(slot < FMTLOG_TELNET_CLIENTS) {
                    clients[slot] = server->accept();
                    // Новый клиент читает с начала истории - её для того и
                    // храним; чтение ничего не забирает у остальных.
                    if(sendHistory)
                        history.rewind(cursors[slot]);
                    else
                        history.seekToEnd(cursors[slot]);
                }
                else {
                    // Мест нет - отказываем, не трогая подключённых.
                    WiFiClient extra = server->accept();
                    extra.stop();
                }
            }

            for(uint8_t i = 0; i < FMTLOG_TELNET_CLIENTS; ++i)
                if(cursors[i].active && clients[i].connected())
                    flush(i);
        }

    } // namespace telnet
} // namespace fmtlog

#endif // ESP8266 || ESP32
