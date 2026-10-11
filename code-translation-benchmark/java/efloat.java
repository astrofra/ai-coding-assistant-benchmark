/*
 * Adapted from the archived CFR 0.152 reconstruction; see documentation/restoration-log.md.
 */
class efloat {
    long t = -1L;
    long dur;
    public boolean donep = false;
    float a;
    float b;
    float cx;

    public efloat(int mdur) {
        this.dur = mdur;
    }

    public void rewind() {
        this.t = -1L;
        this.donep = false;
    }

    public void next(float c) {
        this.a = this.cx;
        this.b = c;
        this.rewind();
        this.cx = c;
    }

    public efloat(float ia, float ib, int idur) {
        this.set(ia, ib, idur);
    }

    public void set(float ia, float ib, int idur) {
        this.a = ia;
        this.b = ib;
        this.dur = idur;
        this.cx = this.a;
    }

    public float val() {
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
        this.cx = tostartp ? (float)top * (this.a - this.b) / (float)bot + this.b : (float)top * (this.b - this.a) / (float)bot + this.a;
        return this.cx;
    }
}
