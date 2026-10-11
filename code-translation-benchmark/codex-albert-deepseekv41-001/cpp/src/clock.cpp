#include "clock.h"

#include <chrono>
#include <ctime>

namespace Clock {

long long time_ms = 0;
bool has_fixed_date = false;
Date fixed_date;
std::ostream* geometry = nullptr;

static long long g_epoch_ms = 0;   // System.currentTimeMillis()
static long long g_origin_ns = 0;  // System.nanoTime()

static long long wall_ms() {
    using namespace std::chrono;
    return (long long)duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

static long long mono_ns() {
    using namespace std::chrono;
    return (long long)duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

void init() {
    g_epoch_ms = wall_ms();
    g_origin_ns = mono_ns();
}

long long now() { return time_ms; }

void tick() { time_ms = g_epoch_ms + (mono_ns() - g_origin_ns) / 1000000LL; }

Date date() {
    if (has_fixed_date) return fixed_date;
    std::time_t tt = std::time(nullptr);
    std::tm lt;
#if defined(_WIN32)
    localtime_s(&lt, &tt);
#else
    localtime_r(&tt, &lt);
#endif
    return Date(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
}

}  // namespace Clock
