// Counterpart of source/cfr/efloat.java; Java zero initialization is explicit.
#pragma once
#include <cstdint>
class efloat {
public:
    std::int64_t t = -1, dur = 0;
    bool donep = false;
    float a = 0, b = 0, cx = 0;
    explicit efloat(int mdur);
    efloat(float ia, float ib, int idur);
    void rewind();
    void next(float c);
    void set(float ia, float ib, int idur);
    float val();
};
