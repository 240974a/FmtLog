// Журнал на веб-страницу.
//
//     #include <FmtLog.h>
//     #include <FmtWeb.h>
//
//     void setup() {
//         WiFi.begin(ssid, pass);
//         web::begin();              // страница на порту 80
//         log::addSink(web::sink);
//     }
//
//     void loop() {
//         web::handle();             // звать в каждом проходе цикла
//     }
//
// Плата отдаёт только поток строк по адресу /log. Сама страница лежит у вас
// на диске - откройте extras/log.html в браузере и укажите адрес платы.
//
// Так страницу можно править, не перепрошивая плату, и она не занимает флеш.
//
// Строки идут по SSE - это обычный HTTP-ответ, который не закрывают. Для
// одностороннего потока он проще WebSocket: не нужны ни рукопожатие с
// кадрированием, ни отдельная библиотека, а при обрыве связи браузер
// переподключается сам.
//
// Заголовок отдельный: приложению без веб-страницы он не достанется даже
// мёртвым кодом.
#pragma once

#if !defined(ESP8266) && !defined(ESP32)
#error "FmtWeb.h needs WiFi: it works on ESP8266 and ESP32 only"
#endif

#include "FmtHistory.h"
#include "Log.h"

namespace fmtlog {
    namespace web {

        // Приёмник журнала - его и передавать в log::addSink.
        void sink(const Record& record);

        // Поднимает веб-сервер. Звать после подъёма сети.
        void begin(uint16_t port = 80);

        // Принимает запросы и досылает подключённым новые строки.
        // Звать в каждом проходе loop().
        void handle();

        // Закрывает сервер и все соединения.
        void end();

        // Сколько вкладок сейчас открыто.
        uint8_t clientCount();

    } // namespace web
} // namespace fmtlog
