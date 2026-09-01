// Как научить библиотеку своему типу: одна специализация formatter<T>.
#include <FmtLog.h>

using namespace fmtlog;

struct Temperature {
    float celsius;
    bool valid;
};

// Правило вывода: тип печатается так, как удобно читать в журнале.
// Внутри доступен тот же format() - вложенные образцы разрешены.
template<>
struct fmtlog::formatter<Temperature> {
    static void format(Fmt& out, const Temperature& value) {
        if(!value.valid) {
            out.format(F("--"));
            return;
        }
        out.format(F("{} °C"), value.celsius);
    }
};

void setup() {
    Serial.begin(115200);
    log::addSink(log::serialSink);

    const Temperature boiler{54.25f, true};
    const Temperature broken{0.0f, false};

    // Дальше тип подставляется как любой встроенный.
    log::info(F("бойлер {}, запасной датчик {}"), boiler, broken);
}

void loop() {
}
