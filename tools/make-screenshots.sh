#!/usr/bin/env bash
# Пересобирает снимки цветного вывода для README.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

build() { # $1 - исходник демонстрации, $2 - имя выходного файла
    g++ -O1 -std=c++17 -I "$root/src" -I "$root/test/mocks" \
        "$work/$1" "$root/src/Fmt.cpp" "$root/src/FmtColor.cpp" "$root/src/Log.cpp" \
        -o "$work/demo"
    "$work/demo" > "$work/raw.txt"
    python3 "$root/tools/ansi-to-svg.py" < "$work/raw.txt" > "$root/docs/$2"
    echo "  docs/$2"
}

# --- пример из шапки README ---
cat > "$work/head.cpp" <<'CPP'
#include "Arduino.h"
#include "FmtColor.h"
#include <cstdio>
using namespace fmtlog;
int main() {
    const char* const names[] = {"app"};
    const uint8_t colors[] = {color::white};
    log::addSink(color::serialSink);
    log::setSourceNames(names, 1);
    color::setSourceColors(colors, 1);
    log::setTime(1788438645u, 204);   // 2026-09-03 12:30:45.204 UTC
    log::info("pin {} = {}, temperature {} C", 13, true, 54.25);
    fputs(Serial.captured().c_str(), stdout);
}
CPP

# --- все уровни в главе про цвет ---
cat > "$work/full.cpp" <<'CPP'
#include "Arduino.h"
#include "FmtColor.h"
#include <cstdio>
using namespace fmtlog;
int main() {
    const char* const names[] = {"app", "net", "sensor"};
    const uint8_t colors[] = {color::white, color::cyan, color::green};
    log::addSink(color::serialSink);
    log::setSourceNames(names, 3);
    color::setSourceColors(colors, 3);
    log::setLevel(Level::trace);

    // Пока время не задано - счёт от запуска, той же ширины.
    log::traceFrom(0, "tick {}", 4096);
    log::infoFrom(0, "started, version {}", "1.2.0");

    log::setTime(1788438645u, 104);   // 2026-09-03 12:30:45.104 UTC

    log::debugFrom(1, "received {} bytes", 128);
    log::infoFrom(2, "boiler {} C", 54.25);
    log::warningFrom(1, "no answer {} times in a row", 3);
    log::errorFrom(2, "sensor is silent");
    fputs(Serial.captured().c_str(), stdout);
}
CPP

echo "building screenshots:"
build head.cpp log-example.svg
build full.cpp log-output.svg
echo "done"
