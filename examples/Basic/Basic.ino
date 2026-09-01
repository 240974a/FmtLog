// Самое начало: собрать строку по образцу и записать в журнал.
#include <FmtLog.h>

using namespace fmtlog;

void setup() {
    Serial.begin(115200);
    log::addSink(log::serialSink);
    log::setLevel(Level::info);

    const int pin = 13;
    const bool level = true;

    // Место каждого значения помечено {} - текст сообщения виден целиком.
    log::info(F("пин {} = {}"), pin, level);

    // Строку можно собрать и без журнала, в свой буфер.
    char line[64];
    Fmt out(line, sizeof(line));
    out.format(F("напряжение {} В, ток {} А"), 3.28, 0.42);
    Serial.println(line);

    // Значение печатается, только если оно есть: условие пишется один раз.
    const bool hasName = false;
    log::info(F("устройство{} готово"), opt(hasName, F(" '"), "sensor-1", F("'")));
}

void loop() {
}
