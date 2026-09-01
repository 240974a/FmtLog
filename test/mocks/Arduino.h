// Заглушка Arduino для тестов на машине разработчика.
//
// Содержит только то, чем пользуется библиотека: работу с флешем, dtostrf и
// millis(). На плате вместо неё берётся настоящий заголовок ядра.
#pragma once

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>

// На машине разработчика вся память одинакова, поэтому "чтение из флеша" -
// обычное разыменование.
class __FlashStringHelper;
#define F(string_literal) (reinterpret_cast<const __FlashStringHelper*>(string_literal))
#define PROGMEM
#define pgm_read_byte(addr) (*reinterpret_cast<const uint8_t*>(addr))

inline char* dtostrf(double value, signed char width, unsigned char precision, char* buffer) {
    char format[16];
    snprintf(format, sizeof(format), "%%%d.%df", static_cast<int>(width),
             static_cast<int>(precision));
    sprintf(buffer, format, value);
    return buffer;
}

// Время на машине разработчика: растёт от вызова к вызову, чтобы записи
// журнала не выглядели одномоментными. Тесты на его значение не опираются.
inline uint32_t& millisValue() {
    static uint32_t value = 12104;
    return value;
}

inline uint32_t millis() {
    const uint32_t now = millisValue();
    millisValue() += 176 + (now % 7) * 61;
    return now;
}

// Урезанный String: библиотеке нужны только c_str() и length().
class String {
  public:
    String() = default;
    String(const char* text) : value_(text ? text : "") {
    }
    const char* c_str() const {
        return value_.c_str();
    }
    size_t length() const {
        return value_.length();
    }

  private:
    std::string value_;
};

// Приёмник печати: заменяет Serial в тестах, накапливая вывод в строке.
class Print {
  public:
    virtual ~Print() = default;
    void print(const char* text) {
        out_ += text ? text : "";
    }
    void print(const __FlashStringHelper* text) {
        out_ += reinterpret_cast<const char*>(text);
    }
    void print(char symbol) {
        out_ += symbol;
    }
    void print(unsigned int value) {
        out_ += std::to_string(value);
    }
    void print(uint8_t value) {
        out_ += std::to_string(value);
    }
    void println() {
        out_ += '\n';
    }
    void println(const char* text) {
        print(text);
        println();
    }

    const std::string& captured() const {
        return out_;
    }
    void reset() {
        out_.clear();
    }

  private:
    std::string out_;
};

inline Print Serial;
