// Counterpart of source/cfr/eint.java; Java zero initialization is explicit.
#pragma once
#include <cstdint>
class eint {
public:
    std::int64_t t = -1, dur = 0;
    bool donep = false;
    int a = 0, b = 0, cx = 0;
    explicit eint(int mdur);
    eint(int ia, int ib, int idur);
    void rewind();
    void next(int c);
    void set(int ia, int ib, int idur);
    int val();
};
