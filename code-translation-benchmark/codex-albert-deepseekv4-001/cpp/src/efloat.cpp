// Translation of source/cfr/efloat.java.
#include "efloat.hpp"
#include "host.hpp"
#include "java_semantics.hpp"

efloat::efloat(int mdur) { this->dur = mdur; }
void efloat::rewind() { this->t = -1L; this->donep = false; }
void efloat::next(float c) {
    this->a = this->cx;
    this->b = c;
    this->rewind();
    this->cx = c;
}
efloat::efloat(float ia, float ib, int idur) { this->set(ia, ib, idur); }
void efloat::set(float ia, float ib, int idur) {
    this->a = ia; this->b = ib; this->dur = idur; this->cx = this->a;
}
float efloat::val() {
    if (this->t == -1L) {
        this->t = host::currentTimeMillis();
        return this->a;
    }
    std::int64_t nt = host::currentTimeMillis();
    std::int64_t dt = java::sub64(nt, this->t);
    if (dt >= this->dur) {
        this->donep = true;
        this->cx = this->b;
        return this->b;
    }
    bool tostartp = false;
    dt = java::sub64(this->dur, dt);
    std::int64_t top = java::mul64(java::mul64(dt, dt), dt);
    std::int64_t bot = java::mul64(java::mul64(this->dur, this->dur), this->dur);
    tostartp = true;
    this->cx = tostartp ? (float)top * (this->a - this->b) / (float)bot + this->b
                        : (float)top * (this->b - this->a) / (float)bot + this->a;
    return this->cx;
}
