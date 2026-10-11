/*
 * Adapted from the archived CFR 0.152 reconstruction; see documentation/restoration-log.md.
 */
class eint {
    long t = -1L;
    long dur;
    public boolean donep = false;
    int a;
    int b;
    int cx;

    public void rewind() {
        this.t = -1L;
        this.donep = false;
    }

    public void next(int c) {
        this.a = this.cx;
        this.b = c;
        this.rewind();
        this.cx = c;
    }

    public eint(int mdur) {
        this.dur = mdur;
    }

    public eint(int ia, int ib, int idur) {
        this.set(ia, ib, idur);
    }

    public void set(int ia, int ib, int idur) {
        this.a = ia;
        this.b = ib;
        this.dur = idur;
        this.cx = this.a;
    }

    public int val() {
        if (this.t == -1L) {
            this.t = FloraClock.now();
            return this.a;
        }
        long nt = FloraClock.now();
        long dt = nt - this.t;
        if (dt >= this.dur) {
            this.donep = true;
            this.cx = this.b;
            return this.b;
        }
        boolean tostartp = false;
        dt = this.dur - dt;
        long top = dt * dt * dt;
        long bot = this.dur * this.dur * this.dur;
        tostartp = true;
        this.cx = tostartp ? (int)(top * (long)(this.a - this.b) / bot + (long)this.b) : (int)(top * (long)(this.b - this.a) / bot + (long)this.a);
        return this.cx;
    }
}
