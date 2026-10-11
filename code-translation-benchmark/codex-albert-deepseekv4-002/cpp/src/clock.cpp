#include "clock.h"

#include <chrono>
#include <ctime>

namespace flora {

std::FILE* Clock::geometry = nullptr;
std::int64_t Clock::epoch_ = 0;
std::int64_t Clock::origin_ns_ = 0;
std::int64_t Clock::time_ = 0;
bool Clock::date_fixed_ = false;
Date Clock::fixed_date_;

void Clock::init() {
    using namespace std::chrono;
    epoch_ = duration_cast<milliseconds>(
                 system_clock::now().time_since_epoch()).count();
    origin_ns_ = duration_cast<nanoseconds>(
                     steady_clock::now().time_since_epoch()).count();
}

void Clock::tick() {
    using namespace std::chrono;
    std::int64_t now_ns =
        duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
    time_ = epoch_ + (now_ns - origin_ns_) / 1000000L;
}

std::int64_t Clock::now() { return time_; }

Date Clock::date() {
    if (date_fixed_) return fixed_date_;
    std::time_t now = std::time(nullptr);
    std::tm* tm = std::localtime(&now);
    Date d;
    d.year = tm->tm_year + 1900;
    d.month = tm->tm_mon + 1;
    d.day = tm->tm_mday;
    return d;
}

void Clock::setTime(std::int64_t t) { time_ = t; }

void Clock::setFixedDate(const Date& d) {
    date_fixed_ = true;
    fixed_date_ = d;
}

bool Clock::hasFixedDate() { return date_fixed_; }

}  // namespace flora
