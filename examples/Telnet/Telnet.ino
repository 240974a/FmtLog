// Журнал по сети: подключитесь telnet-ом к плате и читайте её вывод.
//
//     telnet <адрес платы>
//
// Подключившийся видит не только новые сообщения, но и накопленную историю.
// У каждого соединения своё место в ней, поэтому второй и третий клиент
// получают ту же глубину, что и первый: чтение ничего не стирает.
//
// Строка идёт та же, что в порту, и с цветом - терминал понимает те же
// последовательности ANSI. Убрать цвет: color::setEnabled(false).
//
// Пример для ESP8266 и ESP32 - на AVR нет сети.
#include <FmtLog.h>
#include <FmtTelnet.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

using namespace fmtlog;

const char* ssid = "your-network";
const char* password = "your-password";

enum Source : uint8_t { app, net };

const char* const kNames[] = {"app", "net"};

void setup() {
    Serial.begin(115200);
    log::setSourceNames(kNames, 2);
    log::addSink(log::serialSink);

    WiFi.begin(ssid, password);
    while(WiFi.status() != WL_CONNECTED)
        delay(200);

    telnet::begin();
    log::addSink(telnet::sink);

    log::infoFrom(net, F("telnet ready at {}"), WiFi.localIP());
}

void loop() {
    telnet::handle();

    // Что-нибудь для истории: подключившийся позже увидит и это.
    static uint32_t last = 0;
    if(millis() - last > 2000) {
        last = millis();
        log::infoFrom(app, F("uptime {} s, telnet clients {}"),
                      millis() / 1000, telnet::clientCount());
    }
}
