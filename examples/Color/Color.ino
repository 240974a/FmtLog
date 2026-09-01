// Цветной вывод в терминал.
//
// Уровень и источник раскрашиваются, поэтому ошибка не теряется среди
// отладочных строк.
//
// Монитор порта в Arduino IDE управляющих последовательностей ANSI не
// понимает и показывает их как мусор - там нужен обычный log::serialSink.
// Терминал PlatformIO, screen, minicom и putty цвет показывают.
#include <FmtColor.h>
#include <FmtLog.h>

using namespace fmtlog;

enum Source : uint8_t { app, net, sensor };

const char* const kNames[] = {"app", "net", "sensor"};

// Свой цвет каждому источнику: поток из разных частей приложения читается
// с одного взгляда. Уровни раскрашиваются сами.
const uint8_t kColors[] = {color::white, color::cyan, color::green};

void setup() {
    Serial.begin(115200);

    log::setSourceNames(kNames, 3);
    color::setSourceColors(kColors, 3);

    // Цветной приёмник вместо обычного log::serialSink.
    log::addSink(color::serialSink);
    log::setLevel(Level::trace);

    log::traceFrom(app, F("такт {}"), millis());
    log::debugFrom(net, F("принято {} байт"), 128);
    log::infoFrom(app, F("запуск, версия {}"), F("1.0.0"));
    log::infoFrom(sensor, F("бойлер {} °C"), 54.25);
    log::warningFrom(net, F("нет ответа {} раз подряд"), 3);
    log::errorFrom(sensor, F("датчик не отвечает"));

    // Раскраску можно снять на ходу, не меняя приёмник.
    color::setEnabled(false);
    log::infoFrom(app, F("дальше без цвета"));
    color::setEnabled(true);

    // Цвета уровней тоже заменяются - массив на пять значений, от trace до error.
    static const uint8_t kQuiet[] = {color::darkGray, color::darkGray, color::gray,
                                     color::yellow, color::red};
    color::setLevelColors(kQuiet);
    log::infoFrom(app, F("сдержанная палитра"));
}

void loop() {
}
