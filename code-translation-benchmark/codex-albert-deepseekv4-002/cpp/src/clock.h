/*
 * Port of the Java FloraClock. One coherent monotonic clock per frame.
 * `time` starts at 0 until the first tick(); fixtures set and freeze it.
 */
#ifndef FLORA_CLOCK_H
#define FLORA_CLOCK_H

#include <cstdint>
#include <cstdio>
#include <string>

namespace flora {

struct Date {
    int year;
    int month;   // 1..12
    int day;     // 1..31

    Date() : year(2024), month(1), day(1) {}
};

class Clock {
public:
    // One-time monotonic epoch capture.
    static void init();

    // Advance `time` to wall time. In fixtures time is set directly and frozen.
    static void tick();

    // Returns the current logical time in milliseconds.
    static std::int64_t now();

    // Optional pinned date; otherwise the real local date.
    static Date date();

    static void setTime(std::int64_t t);
    static void setFixedDate(const Date& d);
    static bool hasFixedDate();

    // Geometry trace output (only non-null during fixture rendering).
    static std::FILE* geometry;

private:
    static std::int64_t epoch_;
    static std::int64_t origin_ns_;
    static std::int64_t time_;
    static bool date_fixed_;
    static Date fixed_date_;
};

}  // namespace flora

#endif  // FLORA_CLOCK_H
