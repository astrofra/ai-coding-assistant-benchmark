/*
 * Adapted from the archived CFR 0.152 reconstruction; see documentation/restoration-log.md.
 */
import java.awt.Color;
import java.awt.Graphics;

class Ctm {
    public float[] gctm = new float[9];
    int cp;
    final int N = 16;
    public float[][] ctms = new float[16][9];
    int penx;
    int peny;
    int ONEHALF = 32768;
    int[] xpts = new int[256];
    int[] ypts = new int[256];
    int growchunk = 256;
    int npts;
    Graphics gr;

    void MultMat3x3(float[] a, float[] b, float[] dest) {
        dest[0] = a[0] * b[0] + a[1] * b[3] + a[2] * b[6];
        dest[1] = a[0] * b[1] + a[1] * b[4] + a[2] * b[7];
        dest[2] = 0.0f;
        dest[3] = a[3] * b[0] + a[4] * b[3] + a[5] * b[6];
        dest[4] = a[3] * b[1] + a[4] * b[4] + a[5] * b[7];
        dest[5] = 0.0f;
        dest[6] = a[6] * b[0] + a[7] * b[3] + a[8] * b[6];
        dest[7] = a[6] * b[1] + a[7] * b[4] + a[8] * b[7];
        dest[8] = 1.0f;
    }

    void identityctm(float[] a) {
        a[0] = 1.0f;
        a[1] = 0.0f;
        a[2] = 0.0f;
        a[3] = 0.0f;
        a[4] = 1.0f;
        a[5] = 0.0f;
        a[6] = 0.0f;
        a[7] = 0.0f;
        a[8] = 1.0f;
    }

    void CopyMat3x3(float[] a, float[] b) {
        b[0] = a[0];
        b[1] = a[1];
        b[2] = a[2];
        b[3] = a[3];
        b[4] = a[4];
        b[5] = a[5];
        b[6] = a[6];
        b[7] = a[7];
        b[8] = a[8];
    }

    void MakeTranslationMatrix(float[] a, float dx, float dy) {
        a[0] = 1.0f;
        a[1] = 0.0f;
        a[2] = 0.0f;
        a[3] = 0.0f;
        a[4] = 1.0f;
        a[5] = 0.0f;
        a[6] = dx;
        a[7] = dy;
        a[8] = 1.0f;
    }

    void MakeScaleMatrix(float[] a, float sx, float sy) {
        a[0] = sx;
        a[1] = 0.0f;
        a[2] = 0.0f;
        a[3] = 0.0f;
        a[4] = sy;
        a[5] = 0.0f;
        a[6] = 0.0f;
        a[7] = 0.0f;
        a[8] = 1.0f;
    }

    void MakeRotationMatrix(float[] a, float rtheta) {
        double theta = (double)rtheta * Math.PI / 180.0;
        a[0] = (float)Math.cos(theta);
        a[1] = (float)Math.sin(theta);
        a[2] = 0.0f;
        a[3] = -((float)Math.sin(theta));
        a[4] = (float)Math.cos(theta);
        a[5] = 0.0f;
        a[6] = 0.0f;
        a[7] = 0.0f;
        a[8] = 1.0f;
    }

    void MakeSkewMatrix(float[] a, float rtheta, float rtheta2) {
        double theta = (double)rtheta * Math.PI / 180.0;
        double theta2 = (double)rtheta2 * Math.PI / 180.0;
        a[0] = 1.0f;
        a[1] = (float)Math.tan(theta);
        a[2] = 0.0f;
        a[3] = -((float)Math.tan(theta2));
        a[4] = 1.0f;
        a[5] = 0.0f;
        a[6] = 0.0f;
        a[7] = 0.0f;
        a[8] = 1.0f;
    }

    void rotctm(float[] a, float theta) {
        float[] scratch = new float[9];
        float[] rotmat = new float[9];
        this.CopyMat3x3(a, scratch);
        this.MakeRotationMatrix(rotmat, theta);
        this.MultMat3x3(rotmat, scratch, a);
    }

    void skewctm(float[] a, float theta, float theta2) {
        float[] scratch = new float[9];
        float[] rotmat = new float[9];
        this.CopyMat3x3(a, scratch);
        this.MakeSkewMatrix(rotmat, theta, theta2);
        this.MultMat3x3(rotmat, scratch, a);
    }

    void transctm(float[] a, float dx, float dy) {
        float[] scratch = new float[9];
        float[] xlatemat = new float[9];
        this.CopyMat3x3(a, scratch);
        this.MakeTranslationMatrix(xlatemat, dx, dy);
        this.MultMat3x3(xlatemat, scratch, a);
    }

    void scalectm(float[] a, float sx, float sy) {
        float[] scratch = new float[9];
        float[] scalemat = new float[9];
        this.CopyMat3x3(a, scratch);
        this.MakeScaleMatrix(scalemat, sx, sy);
        this.MultMat3x3(scalemat, scratch, a);
    }

    public void init() {
        this.cp = 0;
        this.identityctm(this.gctm);
        this.CopyMat3x3(this.gctm, this.ctms[0]);
    }

    public Ctm() {
        this.init();
    }

    public void push() {
        if (this.cp >= 16) {
            return;
        }
        this.CopyMat3x3(this.gctm, this.ctms[this.cp]);
        ++this.cp;
        this.CopyMat3x3(this.gctm, this.ctms[this.cp]);
    }

    public void pop() {
        --this.cp;
        if (this.cp < 0) {
            return;
        }
        this.CopyMat3x3(this.ctms[this.cp], this.gctm);
    }

    public void rotate(float theta) {
        this.rotctm(this.gctm, theta);
    }

    public void skew(float theta, float theta2) {
        this.skewctm(this.gctm, theta, theta2);
    }

    public void translate(float dx, float dy) {
        this.transctm(this.gctm, dx, dy);
    }

    public void scale(float sx, float sy) {
        this.scalectm(this.gctm, sx, sy);
    }

    public void moveto(float x, float y) {
        this.penx = this.x(x, y);
        this.peny = this.y(x, y);
        this.npts = 0;
        this.addpt(this.penx, this.peny);
    }

    public void addpt(int x, int y) {
        this.penx = x;
        this.peny = y;
        if (this.npts > this.xpts.length - 16) {
            int len = this.xpts.length;
            System.out.println("*** Growing internal bezier cache larger to total " + (len + this.growchunk));
            int[] a = new int[len + this.growchunk];
            int[] b = new int[len + this.growchunk];
            int i = 0;
            while (i < len) {
                a[i] = this.xpts[i];
                b[i] = this.ypts[i];
                ++i;
            }
            this.ypts = null;
            this.xpts = null;
            this.xpts = a;
            this.ypts = b;
            System.out.println("**> Successful.");
        }
        this.xpts[this.npts] = this.penx;
        this.ypts[this.npts++] = this.peny;
    }

    public void curveto(float p2x, float p2y, float p3x, float p3y, float p4x, float p4y) {
        int a = this.x(p2x, p2y);
        int b = this.y(p2x, p2y);
        int c = this.x(p3x, p3y);
        int d = this.y(p3x, p3y);
        int e = this.x(p4x, p4y);
        int f = this.y(p4x, p4y);
        this.BCurve(this.penx << 16, this.peny << 16, a << 16, b << 16, c << 16, d << 16, e << 16, f << 16);
        this.addpt(e, f);
    }

    public void lineto(float x, float y) {
        this.addpt(this.x(x, y), this.y(x, y));
    }

    public void strokeandfill(Graphics g, Color cstroke, Color cfill) {
        g.setColor(cfill);
        this.gr = g;
        this.gr.fillPolygon(this.xpts, this.ypts, this.npts);
        g.setColor(cstroke);
        this.stroke(g);
    }

    public void fill(Graphics g) {
        if (FloraClock.geometry != null) {
            FloraClock.geometry.print((g.getColor().getRGB() & 0xffffff) + " " + this.npts);
            for (int i = 0; i < this.npts; i++)
                FloraClock.geometry.print(" " + this.xpts[i] + " " + this.ypts[i]);
            FloraClock.geometry.println();
        }
        this.gr = g;
        this.gr.fillPolygon(this.xpts, this.ypts, this.npts);
        this.npts = 0;
    }

    public void stroke(Graphics g) {
        this.gr = g;
        this.gr.drawPolygon(this.xpts, this.ypts, this.npts);
        this.npts = 0;
    }

    public void BCurve(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4) {
        int y34;
        int y234;
        int y23;
        int y12;
        int y123;
        int y1234;
        int dy;
        int x12 = x1 + x2 >> 1;
        int x23 = x2 + x3 >> 1;
        int x123 = x12 + x23 >> 1;
        int x34 = x3 + x4 >> 1;
        int x234 = x23 + x34 >> 1;
        int x1234 = x123 + x234 >> 1;
        int dx = (x1 < x1234 ? x1234 - x1 : x1 - x1234) >> 16;
        if (dx + (dy = (y1 < (y1234 = (y123 = (y12 = y1 + y2 >> 1) + (y23 = y2 + y3 >> 1) >> 1) + (y234 = y23 + (y34 = y3 + y4 >> 1) >> 1) >> 1) ? y1234 - y1 : y1 - y1234) >> 16) > 10) {
            this.BCurve(x1, y1, x12, y12, x123, y123, x1234, y1234);
        }
        int newx = x1234 + this.ONEHALF >> 16;
        int newy = y1234 + this.ONEHALF >> 16;
        this.penx = newx;
        this.peny = newy;
        this.addpt(this.penx, this.peny);
        dx = (x1234 < x4 ? x4 - x1234 : x1234 - x4) >> 16;
        dy = (y1234 < y4 ? y4 - y1234 : y1234 - y4) >> 16;
        if (dx + dy > 10) {
            this.BCurve(x1234, y1234, x234, y234, x34, y34, x4, y4);
        }
    }

    public int x(float x, float y) {
        return (int)(x * this.gctm[0] + y * this.gctm[3] + this.gctm[6] + 0.5f);
    }

    public int xsc(float x) {
        return (int)(x * this.gctm[0] + 0.5f);
    }

    public int ysc(float x) {
        return (int)(x * this.gctm[4] + 0.5f);
    }

    public int y(float x, float y) {
        return (int)(x * this.gctm[1] + y * this.gctm[4] + this.gctm[7] + 0.5f);
    }
}
