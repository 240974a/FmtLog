// Уведомления по SNMP.
//
// В журнал идёт всё подряд, а дежурному уходит только то, что требует
// вмешательства: err и выше. Такое событие приходит к нему trap'ом на порт
// 162 - туда, где его ждёт Zabbix, PRTG или другой монитор.
//
// Проверить без монитора можно так:
//
//     snmptrapd -f -Lo -c /dev/null 162
//
// Каждый trap напечатается в консоли вместе с уровнем и источником.

// Пример для ESP8266 и ESP32 - на AVR нет сети.
#include <FmtLog.h>
#include <FmtSnmp.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

using namespace fmtlog;

const char* ssid = "your-network";
const char* password = "your-password";

// Куда слать уведомления.
const IPAddress monitor(192, 168, 1, 10);

enum Source : uint8_t { app, net, boiler };
const char* const kSources[] = {"app", "net", "boiler"};

void setup() {
    Serial.begin(115200);
    log::setSourceNames(kSources, 3);
    log::addSink(log::serialSink);

    WiFi.begin(ssid, password);
    while(WiFi.status() != WL_CONNECTED)
        delay(500);

    snmp::begin(monitor);
    // Своя ветка OID: у организации она обычно уже есть. По умолчанию берётся
    // ветка для проб, её хватает, пока монитор настраивают.
    snmp::setEnterpriseOid("1.3.6.1.4.1.12345");
    log::addSink(snmp::sink);

    log::systemFrom(app, F("started, notifications go to SNMP"));
}

void loop() {
    snmp::handle();

    static uint32_t last = 0;
    if(millis() - last < 10000)
        return;
    last = millis();

    // Это останется в журнале и до монитора не дойдёт.
    log::infoFrom(boiler, F("temperature {} C"), 54.25);

    // А это уйдёт trap'ом.
    static uint8_t silence = 0;
    if(++silence >= 3) {
        silence = 0;
        log::errFrom(net, F("sensor is silent for {} s"), 30);
    }

    // Сколько событий не уместилось в очередь, пока сеть была недоступна.
    if(snmp::lost())
        log::warnFrom(app, F("{} notifications lost"), snmp::lost());
}
