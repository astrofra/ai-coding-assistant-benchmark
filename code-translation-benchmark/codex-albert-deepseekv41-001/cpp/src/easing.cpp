#include "easing.h"

#include "clock.h"

float Efloat::val() {
    if (t == -1) {
        t = Clock::now();
        return a;
    }
    long long nt = Clock::now();
    long long dt = nt - t;
    if (dt >= dur) {
        donep = true;
        cx = b;
        return b;
    }
    dt = dur - dt;
    long long top = dt * dt * dt;
    long long bot = dur * dur * dur;
    // (float)top * (a - b) / (float)bot + b, evaluated strictly in float.
    cx = (float)top * (a - b) / (float)bot + b;
    return cx;
}

int Eint::val() {
    if (t == -1) {
        t = Clock::now();
        return a;
    }
    long long nt = Clock::now();
    long long dt = nt - t;
    if (dt >= dur) {
        donep = true;
        cx = b;
        return b;
    }
    dt = dur - dt;
    long long top = dt * dt * dt;
    long long bot = dur * dur * dur;
    // (int)(top * (long)(a - b) / bot + (long)b): integer divide truncates toward zero.
    cx = (int)(top * (long long)(a - b) / bot + (long long)b);
    return cx;
}
