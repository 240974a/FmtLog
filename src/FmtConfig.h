// Настройки сборки и различия платформ.
//
// Все значения можно переопределить снаружи - через -D в platformio.ini или
// через #define до включения заголовка библиотеки.
#pragma once

#include <Arduino.h>

// --- различия платформ ----------------------------------------------------

#if defined(ESP8266) || defined(ESP32)
#define FMTLOG_HAS_IPADDRESS 1
#include <IPAddress.h>
#else
// На AVR сети нет, и тянуть IPAddress незачем.
#define FMTLOG_HAS_IPADDRESS 0
#endif

// На AVR <type_traits> нет в стандартной поставке ядра, поэтому нужные
// признаки определяем сами - их немного.
#if defined(ARDUINO_ARCH_AVR)
namespace std {
    template<typename T, T v>
    struct integral_constant {
        static constexpr T value = v;
    };
    using true_type = integral_constant<bool, true>;
    using false_type = integral_constant<bool, false>;

    template<bool B, typename T = void>
    struct enable_if {};
    template<typename T>
    struct enable_if<true, T> {
        using type = T;
    };

    template<typename A, typename B>
    struct is_same : false_type {};
    template<typename A>
    struct is_same<A, A> : true_type {};

    template<typename T>
    struct is_enum : integral_constant<bool, __is_enum(T)> {};

    template<typename T>
    struct is_integral : false_type {};
    template<> struct is_integral<bool> : true_type {};
    template<> struct is_integral<char> : true_type {};
    template<> struct is_integral<signed char> : true_type {};
    template<> struct is_integral<unsigned char> : true_type {};
    template<> struct is_integral<short> : true_type {};
    template<> struct is_integral<unsigned short> : true_type {};
    template<> struct is_integral<int> : true_type {};
    template<> struct is_integral<unsigned int> : true_type {};
    template<> struct is_integral<long> : true_type {};
    template<> struct is_integral<unsigned long> : true_type {};
    template<> struct is_integral<long long> : true_type {};
    template<> struct is_integral<unsigned long long> : true_type {};

    template<typename T>
    struct is_floating_point : false_type {};
    template<> struct is_floating_point<float> : true_type {};
    template<> struct is_floating_point<double> : true_type {};

    // Знаковость спрашивают и о структурах - для них сравнение T(-1) < T(0)
    // не собралось бы, поэтому по умолчанию отвечаем "нет", а перечисляем
    // только арифметические типы.
    template<typename T>
    struct is_signed : false_type {};
    template<> struct is_signed<signed char> : true_type {};
    template<> struct is_signed<short> : true_type {};
    template<> struct is_signed<int> : true_type {};
    template<> struct is_signed<long> : true_type {};
    template<> struct is_signed<long long> : true_type {};
    template<> struct is_signed<float> : true_type {};
    template<> struct is_signed<double> : true_type {};
    // char знаковый или нет - решает компилятор, поэтому проверяем.
    template<> struct is_signed<char> : integral_constant<bool, (char(-1) < char(0))> {};

    template<typename T>
    struct is_unsigned : false_type {};
    template<> struct is_unsigned<bool> : true_type {};
    template<> struct is_unsigned<unsigned char> : true_type {};
    template<> struct is_unsigned<unsigned short> : true_type {};
    template<> struct is_unsigned<unsigned int> : true_type {};
    template<> struct is_unsigned<unsigned long> : true_type {};
    template<> struct is_unsigned<unsigned long long> : true_type {};
} // namespace std
#else
#include <type_traits>
#endif

// --- настройки ------------------------------------------------------------

// Знаков после запятой при выводе дробных.
#ifndef FMTLOG_FLOAT_DECIMALS
#define FMTLOG_FLOAT_DECIMALS 3
#endif

// Размер буфера под одно сообщение журнала. На AVR памяти мало, поэтому по
// умолчанию буфер вдвое короче.
#ifndef FMTLOG_MESSAGE_SIZE
#if defined(ARDUINO_ARCH_AVR)
#define FMTLOG_MESSAGE_SIZE 64
#else
#define FMTLOG_MESSAGE_SIZE 128
#endif
#endif

// Сколько источников журнала различает приложение. Уровень хранится для
// каждого отдельно, поэтому лишние источники стоят по байту.
#ifndef FMTLOG_SOURCE_COUNT
#define FMTLOG_SOURCE_COUNT 8
#endif

// Порог, ниже которого вызовы журнала выбрасываются на этапе компиляции: их
// образцы и значения в прошивку не попадают вовсе.
//
// Уровни: 0 trace, 1 debug, 2 info, 3 warning, 4 error, 5 none
#ifndef FMTLOG_COMPILE_LEVEL
#define FMTLOG_COMPILE_LEVEL 0
#endif
