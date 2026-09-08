// Журнал на веб-странице.
//
// Плата отдаёт поток строк по адресу /log. Сама страница - extras/log.html -
// лежит на вашем диске: откройте её в браузере и впишите адрес платы.
//
// Есть цвет, поиск, выбор уровня, переход к дате и выгрузка в файл.
//
// Строки идут по SSE - обычному HTTP-ответу, который не закрывают. Для
// одностороннего потока он проще WebSocket: не нужны ни рукопожатие с
// кадрированием, ни отдельная библиотека, а при обрыве связи браузер
// переподключается сам.
//
// Пример для ESP8266 и ESP32 - на платах без сети модуль недоступен.
#include <FmtLog.h>
#include <FmtWeb.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

using namespace fmtlog;

const char* ssid = "your-network";
const char* password = "your-password";

enum Source : uint8_t { app, net, sensor };

const char* const kNames[] = {"app", "net", "sensor"};

void setup() {
    Serial.begin(115200);
    log::setSourceNames(kNames, 3);
    log::addSink(log::serialSink);

    WiFi.begin(ssid, password);
    while(WiFi.status() != WL_CONNECTED)
        delay(200);

    web::begin();
    log::addSink(web::sink);

    log::infoFrom(net, F("log stream at http://{}/log"), WiFi.localIP());
}

void loop() {
    web::handle();

    // Что-нибудь для журнала: открывший страницу позже увидит и это.
    static uint32_t last = 0;
    if(millis() - last > 3000) {
        last = millis();
        log::infoFrom(sensor, F("boiler {} C"), 54.25);
        if(millis() % 15000 < 3000)
            log::warnFrom(net, F("no answer {} times in a row"), 3);
    }
}
