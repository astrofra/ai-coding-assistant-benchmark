// Translation of source/cfr/eint.java.
#include "eint.hpp"
#include "host.hpp"
#include "java_semantics.hpp"

void eint::rewind() { this->t = -1L; this->donep = false; }
void eint::next(int c) {
    this->a = this->cx;
    this->b = c;
    this->rewind();
    this->cx = c;
}
eint::eint(int mdur) { this->dur = mdur; }
eint::eint(int ia, int ib, int idur) { this->set(ia, ib, idur); }
void eint::set(int ia, int ib, int idur) {
    this->a = ia; this->b = ib; this->dur = idur; this->cx = this->a;
}
int eint::val() {
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
    this->cx = tostartp
        ? java::narrow(java::add64(java::div64(java::mul64(top, (std::int64_t)java::sub(this->a, this->b)), bot), this->b))
        : java::narrow(java::add64(java::div64(java::mul64(top, (std::int64_t)java::sub(this->b, this->a)), bot), this->a));
    return this->cx;
}
