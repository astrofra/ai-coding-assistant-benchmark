/*
 * Ports of efloat / eint: symmetric cubic ease over `dur` ms using the clock.
 * Float variants use float math; integer variants use integer division and
 * truncation with wide intermediates, mirroring the Java source exactly.
 */
#ifndef FLORA_EASING_H
#define FLORA_EASING_H

#include <cstdint>

namespace flora {

class EFloat {
public:
    explicit EFloat(int mdur) : dur_(mdur) {}
    EFloat(float ia, float ib, int idur) { set(ia, ib, idur); }

    void rewind();
    void next(float c);
    void set(float ia, float ib, int idur);
    float val();

    bool donep = false;

private:
    std::int64_t t_ = -1;
    std::int64_t dur_;
    float a_ = 0.0f;
    float b_ = 0.0f;
    float cx_ = 0.0f;
};

class EInt {
public:
    explicit EInt(int mdur) : dur_(mdur) {}
    EInt(int ia, int ib, int idur) { set(ia, ib, idur); }

    void rewind();
    void next(int c);
    void set(int ia, int ib, int idur);
    int val();

    bool donep = false;

private:
    std::int64_t t_ = -1;
    std::int64_t dur_;
    int a_ = 0;
    int b_ = 0;
    int cx_ = 0;
};

}  // namespace flora

#endif  // FLORA_EASING_H
