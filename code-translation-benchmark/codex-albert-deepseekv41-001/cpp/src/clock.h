// Port of FloraClock: one coherent clock per frame with deterministic overrides.
#pragma once

#include <ostream>

struct Date {
    int y, m, d;
    Date() : y(1970), m(1), d(1) {}
    Date(int yy, int mm, int dd) : y(yy), m(mm), d(dd) {}
};

namespace Clock {
// Captured once, like the static initialisers in FloraClock.
void init();

// Wall-clock milliseconds since the epoch, or a frozen value in fixture mode.
extern long long time_ms;

// Optional fixed date (used by the fixture driver); if unset, the real local date.
extern bool has_fixed_date;
extern Date fixed_date;

// Optional geometry trace sink (PrintWriter in the Java source).
extern std::ostream* geometry;

long long now();
void tick();
Date date();
}  // namespace Clock
