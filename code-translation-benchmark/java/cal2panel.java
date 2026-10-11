/*
 * Adapted from the archived CFR 0.152 reconstruction; see documentation/restoration-log.md.
 */
import java.awt.Color;
import java.awt.Event;
import java.awt.Graphics;
import java.awt.Image;
import java.awt.Rectangle;
import java.time.LocalDate;
import java.awt.image.BufferedImage;

class cal2panel
extends IdlePanel {
    String[] stmo = new String[]{"JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE", "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"};
    int[] dimo = new int[]{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    Ctm ai = new Ctm();
    int cury;
    int curd;
    int curm;
    int ndays;
    int curdy;
    int firstdy;
    float[] as;
    float[] bs;
    float[] cs;
    float[] ds;
    float[] gs;
    Color[] cols;
    int maxmorphs = 14;
    int nmorphs;
    int morphc;
    int[] es;
    int[] fs;
    int[] hs;
    efloat a;
    efloat b;
    efloat c;
    efloat d;
    efloat g;
    eint e;
    eint f;
    eint h;
    int gdur = 2000;
    eint colr;
    eint colg;
    eint colb;
    int npetals = 14;
    Image shisim;
    int shisimw;
    int shisimh;
    Image maedaim;
    int maedaimw;
    int maedaimh;
    Image[] rims;
    Image[] bims;
    int rimh;
    int bimh;
    int[] rimws = new int[10];
    int[] bimws = new int[10];
    Image im;
    long drawtime;
    long startmorph = -1L;

    public void mvector(float ca, float cb, float cc, float cd, int ce, int cf, float cg, int ch, Color ccol) {
        this.a.next(ca);
        this.b.next(cb);
        this.c.next(cc);
        this.d.next(cd);
        this.e.next(ce);
        this.f.next(cf);
        this.g.next(cg);
        this.h.next(ch);
        this.colr.next(ccol.getRed());
        this.colg.next(ccol.getGreen());
        this.colb.next(ccol.getBlue());
    }

    public void addval(float ca, float cb, float cc, float cd, int ce, int cf, float cg, int ch, Color ccol) {
        if (this.as == null) {
            this.as = new float[this.maxmorphs];
            this.bs = new float[this.maxmorphs];
            this.cs = new float[this.maxmorphs];
            this.ds = new float[this.maxmorphs];
            this.gs = new float[this.maxmorphs];
            this.hs = new int[this.maxmorphs];
            this.es = new int[this.maxmorphs];
            this.fs = new int[this.maxmorphs];
            this.a = new efloat(this.gdur);
            this.b = new efloat(this.gdur);
            this.c = new efloat(this.gdur);
            this.d = new efloat(this.gdur);
            this.g = new efloat(this.gdur);
            this.h = new eint(this.gdur);
            this.e = new eint(this.gdur);
            this.f = new eint(this.gdur);
            this.cols = new Color[this.maxmorphs];
            this.colr = new eint(this.gdur);
            this.colg = new eint(this.gdur);
            this.colb = new eint(this.gdur);
            this.mvector(ca, cb, cc, cd, ce, cf, cg, ch, ccol);
            this.mvector(ca, cb, cc, cd, ce, cf, cg, ch, ccol);
        }
        this.as[this.nmorphs] = ca;
        this.bs[this.nmorphs] = cb;
        this.cs[this.nmorphs] = cc;
        this.ds[this.nmorphs] = cd;
        this.gs[this.nmorphs] = cg;
        this.hs[this.nmorphs] = ch;
        this.es[this.nmorphs] = ce;
        this.fs[this.nmorphs] = cf;
        this.cols[this.nmorphs] = ccol;
        ++this.nmorphs;
    }

    public void morph() {
        ++this.morphc;
        if (this.morphc >= this.nmorphs) {
            this.morphc = 0;
        }
        this.mvector(this.as[this.morphc], this.bs[this.morphc], this.cs[this.morphc], this.ds[this.morphc], this.es[this.morphc], this.fs[this.morphc], this.gs[this.morphc], this.hs[this.morphc], this.cols[this.morphc]);
    }

    public void petal2(Graphics gv, float loarm, float arma, float armb, float rot, int x, int y, float hiarm, int hiy) {
        float ax = (float)x - (float)((double)arma * Math.cos(rot));
        float ay = (float)y - (float)((double)arma * Math.sin(rot));
        float bx = (float)x + (float)((double)armb * Math.cos(rot));
        float by = (float)y + (float)((double)armb * Math.sin(rot));
        this.ai.moveto(0.0f, 0.0f);
        this.ai.curveto(loarm, 0.0f, ax, ay, x, y);
        this.ai.curveto(bx, by, hiarm, hiy, 0.0f, hiy);
        this.ai.curveto(-hiarm, hiy, -bx, by, -x, y);
        this.ai.curveto(-ax, ay, -loarm, 0.0f, 0.0f, 0.0f);
        this.ai.fill(gv);
    }

    public void petal(Graphics gv) {
        float ca = 0.0f;
        float cb = 0.0f;
        float cc = 0.0f;
        float cd = 0.0f;
        float cg = 0.0f;
        int ce = 0;
        int cf = 0;
        int ch = 0;
        long t = FloraClock.now();
        float rotval = (float)(t % 10000L) * 360.0f / 10000.0f;
        Color col = new Color(this.colr.val(), this.colg.val(), this.colb.val());
        int n = this.npetals;
        int i = 1;
        while (i < n) {
            this.ai.push();
            this.ai.rotate((float)(i * 360 / n) + rotval);
            gv.setColor(this.blend(col, Color.black, (float)i / (float)(n - 1)));
            if (i == 1) {
                ca = this.a.val();
                cb = this.b.val();
                cc = this.c.val();
                cd = this.d.val();
                ce = this.e.val();
                cf = this.f.val();
                cg = this.g.val();
                ch = this.h.val();
                this.petal2(gv, ca, cb, cc, cd, ce, cf, cg, ch);
            } else {
                this.petal2(gv, ca, cb, cc, cd, ce, cf, cg, ch);
            }
            this.ai.pop();
            ++i;
        }
    }

    public Color blend(Color c, Color c2, float per) {
        int bn;
        int gn;
        float r = c.getRed();
        float g = c.getGreen();
        float b = c.getBlue();
        float r2 = c2.getRed();
        float g2 = c2.getGreen();
        float b2 = c2.getBlue();
        int rn = (int)(r * per + (1.0f - per) * r2);
        if (rn < 0) {
            rn = 0;
        }
        if (rn > 255) {
            rn = 255;
        }
        if ((gn = (int)(g * per + (1.0f - per) * g2)) < 0) {
            gn = 0;
        }
        if (gn > 255) {
            gn = 255;
        }
        if ((bn = (int)(b * per + (1.0f - per) * b2)) < 0) {
            bn = 0;
        }
        if (bn > 255) {
            bn = 255;
        }
        return new Color(rn, gn, bn);
    }

    public void doinits() {
        FloraClock.now();
        this.addval(15.5242f, 208.079f, 48.3322f, 2.13347f, 84, 295, 13.0f, 257, Color.pink);
        this.addval(35.3553f, 76.0592f, 94.6256f, 2.52134f, 111, 202, 80.1561f, 320, new Color(104, 41, 94));
        this.addval(30.2655f, 72.2772f, 125.575f, -3.84251f, 104, 305, 58.0086f, 171, Color.blue);
        this.addval(43.0f, 83.1865f, 107.517f, 0.321751f, 61, 116, 140.688f, 310, Color.orange);
        this.addval(12.0416f, 160.178f, 64.2806f, 2.64723f, 110, 244, 9.21954f, 218, Color.red);
        this.addval(90.7965f, 152.856f, 15.8114f, 0.566227f, 164, 249, 114.983f, 332, Color.white);
        this.addval(30.0666f, 260.864f, 60.1664f, 0.855933f, 144, 288, 15.2643f, 147, Color.green);
        this.addval(20.6155f, 50.04f, 54.0833f, 1.5153f, 58, 139, 47.8853f, 348, Color.yellow);
    }

    public void drawdate(Graphics gv) {
        char c;
        Rectangle r = this.bounds();
        int x = r.width / 4;
        int y = r.height / 4;
        LocalDate d = FloraClock.date();
        this.curm = d.getMonthValue();
        this.curd = d.getDayOfMonth();
        String s = Integer.toString(this.curm);
        int i = 0;
        while (i < s.length()) {
            c = s.charAt(i);
            gv.drawImage(this.bims[c - 48], x, y, this);
            x += this.bimws[c - 48] + 1;
            ++i;
        }
        gv.setColor(Color.white);
        gv.drawLine((x += 8) - 5, y - this.bimh / 4, x - 5, y + this.bimh + this.bimh / 4);
        s = Integer.toString(this.curd);
        i = 0;
        while (i < s.length()) {
            c = s.charAt(i);
            gv.drawImage(this.rims[c - 48], x, y, this);
            x += this.rimws[c - 48] + 1;
            ++i;
        }
    }

    public cal2panel(Image sim, Image mim, Image[] rim, Image[] bim) {
        this.maedaim = mim;
        this.rims = rim;
        this.bims = bim;
        this.doinits();
        this.shisim = sim;
        this.shisimw = this.shisim.getWidth(this);
        this.shisimh = this.shisim.getHeight(this);
        this.maedaimw = this.maedaim.getWidth(this);
        this.maedaimh = this.maedaim.getHeight(this);
        int i = 0;
        while (i < 10) {
            this.rimws[i] = this.rims[i].getWidth(this);
            this.bimws[i] = this.bims[i].getWidth(this);
            ++i;
        }
        i = 0;
        this.rimh = this.rims[i].getHeight(this);
        this.bimh = this.bims[i].getHeight(this);
    }

    public boolean mouseUp(Event ev, int x, int y) {
        this.startmorph = FloraClock.now();
        this.morph();
        return true;
    }

    public void redraw(Graphics gv) {
        Graphics g;
        Rectangle r = this.bounds();
        long t = FloraClock.now();
        if (r.width > 16) {
            if (this.im == null) {
                this.im = new BufferedImage(r.width, r.height, BufferedImage.TYPE_INT_RGB);
            }
            g = this.im.getGraphics();
        } else {
            g = gv;
        }
        g.setColor(Color.black);
        g.fillRect(0, 0, r.width, r.height);
        g.drawImage(this.shisim, 8, r.height - this.shisimh - 8, this);
        g.drawImage(this.maedaim, r.width - this.maedaimw - 1, 0, this);
        this.drawdate(g);
        this.ai.push();
        this.ai.translate(r.width / 2, r.height / 2);
        int cx = r.width / 2;
        int cy = r.height / 2;
        long dist = (this.mx - cx) * (this.mx - cx) + (this.my - cy) * (this.my - cy);
        float sc = 0.1f + (float)Math.sqrt(dist) / (float)r.width;
        float maxsc = 0.6f;
        if (sc > maxsc) {
            sc = maxsc;
        }
        this.ai.scale(sc, sc);
        this.petal(g);
        this.ai.pop();
        g.setColor(Color.darkGray);
        g.drawRect(0, 0, r.width - 1, r.height - 1);
        if (this.im != null) {
            gv.drawImage(this.im, 0, 0, this);
            g.dispose();
        }
        this.drawtime = FloraClock.now() - t;
    }
}
