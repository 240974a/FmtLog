// Library sources are compiled into the test directly: when building for the
// developer machine PlatformIO does not treat the project root as an Arduino
// library.
#include <Fmt.cpp>   // из .pio/libdeps, путь даёт build_flags
#include "../../src/Log.cpp"
