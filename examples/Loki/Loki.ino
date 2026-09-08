// Журнал в Loki - хранилище логов, которое читают рядом с Prometheus.
//
// Строки уходят пачками и ложатся в Loki с метками: job, instance, уровень и
// источник. По ним потом и ищут:
//
//     {job="boiler"} |= "pump"
//     {job="boiler", level="error"}
//
// Loki с Grafana можно поднять рядом - в extras/loki есть готовый контейнер и
// описание, как его запустить на Windows и Linux.

// Пример для ESP8266 и ESP32 - на AVR нет сети.
#include <FmtLog.h>
#include <FmtLoki.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

using namespace fmtlog;

const char* ssid = "your-network";
const char* password = "your-password";

enum Source : uint8_t { app, net, boiler };
const char* const kSources[] = {"app", "net", "boiler"};

void setup() {
    Serial.begin(115200);
    log::setSourceNames(kSources, 3);
    log::addSink(log::serialSink);

    WiFi.begin(ssid, password);
    while(WiFi.status() != WL_CONNECTED)
        delay(500);

    loki::begin("192.168.1.10");   // порт 3100 по умолчанию

    // Метки, общие для всех строк. Их должно быть немного: в Loki каждое
    // сочетание меток заводит отдельный поток.
    loki::addLabel("job", "boiler");
    loki::addLabel("instance", "kitchen");

    log::addSink(loki::sink);
    log::systemFrom(app, F("started, logs go to Loki"));
}

void loop() {
    loki::handle();

    static uint32_t last = 0;
    if(millis() - last < 3000)
        return;
    last = millis();

    log::infoFrom(boiler, F("temperature {} C"), 54.25);
    log::debugFrom(net, F("sent {} bytes"), 128);

    // Строки, потерянные из-за недоступности Loki, видны сразу.
    if(loki::lost())
        log::warnFrom(app, F("{} lines lost"), loki::lost());
}
