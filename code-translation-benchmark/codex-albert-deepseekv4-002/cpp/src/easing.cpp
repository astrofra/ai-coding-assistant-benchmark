#include "easing.h"

#include "clock.h"

namespace flora {

void EFloat::rewind() {
    t_ = -1;
    donep = false;
}

void EFloat::next(float c) {
    a_ = cx_;
    b_ = c;
    rewind();
    cx_ = c;
}

void EFloat::set(float ia, float ib, int idur) {
    a_ = ia;
    b_ = ib;
    dur_ = idur;
    cx_ = a_;
}

float EFloat::val() {
    if (t_ == -1) {
        t_ = Clock::now();
        return a_;
    }
    std::int64_t nt = Clock::now();
    std::int64_t dt = nt - t_;
    if (dt >= dur_) {
        donep = true;
        cx_ = b_;
        return b_;
    }
    dt = dur_ - dt;
    std::int64_t top = dt * dt * dt;
    std::int64_t bot = dur_ * dur_ * dur_;
    // Java: tostartp is always true here; preserves (a-b) orientation.
    cx_ = (float)top * (a_ - b_) / (float)bot + b_;
    return cx_;
}

void EInt::rewind() {
    t_ = -1;
    donep = false;
}

void EInt::next(int c) {
    a_ = cx_;
    b_ = c;
    rewind();
    cx_ = c;
}

void EInt::set(int ia, int ib, int idur) {
    a_ = ia;
    b_ = ib;
    dur_ = idur;
    cx_ = a_;
}

int EInt::val() {
    if (t_ == -1) {
        t_ = Clock::now();
        return a_;
    }
    std::int64_t nt = Clock::now();
    std::int64_t dt = nt - t_;
    if (dt >= dur_) {
        donep = true;
        cx_ = b_;
        return b_;
    }
    dt = dur_ - dt;
    std::int64_t top = dt * dt * dt;
    std::int64_t bot = dur_ * dur_ * dur_;
    // Java casts long intermediates toward zero (truncation).
    cx_ = (int)(top * (long)(a_ - b_) / bot + (long)b_);
    return cx_;
}

}  // namespace flora
