// Журнал: уровни, источники и свой приёмник вывода.
//
// Пример для ESP8266 и ESP32: на AVR нет IPAddress.
#include <FmtLog.h>

using namespace fmtlog;

// Источники приложения. Номера произвольные - важно лишь, чтобы они совпадали
// с порядком имён ниже.
enum Source : uint8_t { app, net, sensor };

const char* const kSourceNames[] = {"app", "net", "sensor"};

// Свой приёмник: сюда сообщение приходит уже собранным. Библиотека никуда не
// пишет сама - решает приложение. Этот считает ошибки, чтобы показать их
// число на экране или отправить в сеть; с тем же успехом он мог бы мигать
// светодиодом или писать на карту памяти.
uint16_t errorCount = 0;

void countErrors(const Record& record) {
    if(record.level >= Level::error)
        ++errorCount;
}

void setup() {
    Serial.begin(115200);

    log::setSourceNames(kSourceNames, 3);
    log::addSink(log::serialSink);
    log::addSink(countErrors);

    // Общий уровень - info, но сеть пусть говорит подробнее.
    log::setLevel(Level::info);
    log::setLevel(net, Level::debug);

    log::infoFrom(app, F("запуск"));
    log::debugFrom(net, F("отправлено {} байт на {}"), 128, IPAddress(192, 168, 1, 10));

    // Это сообщение не выведется: уровень источника sensor - info.
    log::debugFrom(sensor, F("сырое значение {}"), 512);

    log::errorFrom(sensor, F("датчик не отвечает {} раз подряд"), 3);

    // Приёмник считал ошибки, пока журнал их печатал.
    log::infoFrom(app, F("ошибок с запуска: {}"), errorCount);
}

void loop() {
}
