// Управление уровнями на ходу - например, по команде из порта.
//
// Отдельно показано отсечение на этапе компиляции: если собрать с
//     -D FMTLOG_COMPILE_LEVEL=2
// то вызовы trace и debug исчезнут из прошивки целиком - вместе с образцами.
#include <FmtLog.h>

using namespace fmtlog;

void applyCommand(char command) {
    switch(command) {
    case 'q': log::setLevel(Level::error); break;   // только ошибки
    case 'n': log::setLevel(Level::info); break;    // обычный режим
    case 'v': log::setLevel(Level::debug); break;   // подробно
    case 't': log::setLevel(Level::trace); break;   // всё подряд
    default: return;
    }
    log::info(F("уровень журнала: {}"), static_cast<int>(log::getLevel()));
}

void setup() {
    Serial.begin(115200);
    log::addSink(log::serialSink);
    log::setLevel(Level::info);

    log::info(F("наберите q/n/v/t, чтобы сменить уровень"));
}

void loop() {
    if(Serial.available())
        applyCommand(Serial.read());

    // Проверка уровня идёт до сборки сообщения, поэтому отброшенный вызов
    // почти ничего не стоит - его можно оставлять и в горячем цикле.
    log::trace(F("такт {}"), millis());
    delay(1000);
}
