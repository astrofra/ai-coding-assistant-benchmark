// Ports of the two symmetric cubic easing helpers (efloat / eint).
//
// Formula (see efloat/eint):
//   if (t == -1) { t = now(); return a; }
//   dt = now() - t;
//   if (dt >= dur) { donep = true; cx = b; return b; }
//   dt = dur - dt;
//   cx = dt*dt*dt * (a-b) / (dur*dur*dur) + b;
// efloat uses float math; eint uses long intermediates and truncation.
#pragma once

struct Efloat {
    long long t = -1;
    long long dur = 0;
    bool donep = false;
    float a = 0.0f, b = 0.0f, cx = 0.0f;

    explicit Efloat(int mdur) : dur(mdur) {}
    Efloat(float ia, float ib, int idur) { set(ia, ib, idur); }

    void set(float ia, float ib, int idur) { a = ia; b = ib; dur = idur; cx = a; }
    void rewind() { t = -1; donep = false; }
    void next(float c) { a = cx; b = c; rewind(); cx = c; }
    float val();
};

struct Eint {
    long long t = -1;
    long long dur = 0;
    bool donep = false;
    int a = 0, b = 0, cx = 0;

    explicit Eint(int mdur) : dur(mdur) {}
    Eint(int ia, int ib, int idur) { set(ia, ib, idur); }

    void set(int ia, int ib, int idur) { a = ia; b = ib; dur = idur; cx = a; }
    void rewind() { t = -1; donep = false; }
    void next(int c) { a = cx; b = c; rewind(); cx = c; }
    int val();
};
