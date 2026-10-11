#include "panel.h"

#include <algorithm>
#include <cmath>

#include "clock.h"

Cal2Panel::Cal2Panel(const Assets& a_) {
    shisim = &a_.shiseido;
    maedaim = &a_.maeda2;
    for (int i = 0; i < 10; ++i) {
        rims[i] = &a_.rims[i];
        bims[i] = &a_.bims[i];
    }

    doinits();

    shisimw = shisim->w;
    shisimh = shisim->h;
    maedaimw = maedaim->w;
    maedaimh = maedaim->h;
    for (int i = 0; i < 10; ++i) {
        rimws[i] = rims[i]->w;
        bimws[i] = bims[i]->w;
    }
    rimh = rims[0]->h;
    bimh = bims[0]->h;
}

void Cal2Panel::mvector(float ca, float cb, float cc, float cd, int ce, int cf, float cg, int ch, const Color& ccol) {
    a->next(ca);
    b->next(cb);
    c->next(cc);
    d->next(cd);
    e->next(ce);
    f->next(cf);
    g->next(cg);
    h->next(ch);
    colr->next(ccol.r);
    colg->next(ccol.g);
    colb->next(ccol.b);
}

void Cal2Panel::addval(float ca, float cb, float cc, float cd, int ce, int cf, float cg, int ch, const Color& ccol) {
    if (!allocated) {
        a.reset(new Efloat(gdur));
        b.reset(new Efloat(gdur));
        c.reset(new Efloat(gdur));
        d.reset(new Efloat(gdur));
        g.reset(new Efloat(gdur));
        h.reset(new Eint(gdur));
        e.reset(new Eint(gdur));
        f.reset(new Eint(gdur));
        colr.reset(new Eint(gdur));
        colg.reset(new Eint(gdur));
        colb.reset(new Eint(gdur));
        allocated = true;
        // Quirk preserved: the first preset is pushed twice on allocation.
        mvector(ca, cb, cc, cd, ce, cf, cg, ch, ccol);
        mvector(ca, cb, cc, cd, ce, cf, cg, ch, ccol);
    }
    as[nmorphs] = ca;
    bs[nmorphs] = cb;
    cs[nmorphs] = cc;
    ds[nmorphs] = cd;
    gs[nmorphs] = cg;
    hs[nmorphs] = ch;
    es[nmorphs] = ce;
    fs[nmorphs] = cf;
    cols[nmorphs] = ccol;
    ++nmorphs;
}

void Cal2Panel::morph() {
    ++morphc;
    if (morphc >= nmorphs) morphc = 0;
    mvector(as[morphc], bs[morphc], cs[morphc], ds[morphc], es[morphc], fs[morphc], gs[morphc], hs[morphc], cols[morphc]);
}

void Cal2Panel::doinits() {
    Clock::now();
    addval(15.5242f, 208.079f, 48.3322f, 2.13347f, 84, 295, 13.0f, 257, Color::pink());
    addval(35.3553f, 76.0592f, 94.6256f, 2.52134f, 111, 202, 80.1561f, 320, Color(104, 41, 94));
    addval(30.2655f, 72.2772f, 125.575f, -3.84251f, 104, 305, 58.0086f, 171, Color::blue());
    addval(43.0f, 83.1865f, 107.517f, 0.321751f, 61, 116, 140.688f, 310, Color::orange());
    addval(12.0416f, 160.178f, 64.2806f, 2.64723f, 110, 244, 9.21954f, 218, Color::red());
    addval(90.7965f, 152.856f, 15.8114f, 0.566227f, 164, 249, 114.983f, 332, Color::white());
    addval(30.0666f, 260.864f, 60.1664f, 0.855933f, 144, 288, 15.2643f, 147, Color::green());
    addval(20.6155f, 50.04f, 54.0833f, 1.5153f, 58, 139, 47.8853f, 348, Color::yellow());
}

void Cal2Panel::petal2(Graphics& gv, float loarm, float arma, float armb, float rot, int x, int y, float hiarm, int hiy) {
    float ax = (float)x - (float)((double)arma * std::cos((double)rot));
    float ay = (float)y - (float)((double)arma * std::sin((double)rot));
    float bx = (float)x + (float)((double)armb * std::cos((double)rot));
    float by = (float)y + (float)((double)armb * std::sin((double)rot));
    ai.moveto(0.0f, 0.0f);
    ai.curveto(loarm, 0.0f, ax, ay, (float)x, (float)y);
    ai.curveto(bx, by, hiarm, (float)hiy, 0.0f, (float)hiy);
    ai.curveto(-hiarm, (float)hiy, -bx, by, (float)-x, (float)y);
    ai.curveto(-ax, ay, -loarm, 0.0f, 0.0f, 0.0f);
    ai.fill(gv);
}

void Cal2Panel::petal(Graphics& gv) {
    float ca = 0.0f, cb = 0.0f, cc = 0.0f, cd = 0.0f, cg = 0.0f;
    int ce = 0, cf = 0, ch = 0;
    long long t = Clock::now();
    float rotval = (float)(t % 10000LL) * 360.0f / 10000.0f;
    int rr = colr->val();
    int gg = colg->val();
    int bb = colb->val();
    Color col(rr, gg, bb);
    int n = npetals;
    int i = 1;
    while (i < n) {
        ai.push();
        ai.rotate((float)(i * 360 / n) + rotval);
        gv.setColor(blend(col, Color::black(), (float)i / (float)(n - 1)));
        if (i == 1) {
            ca = a->val();
            cb = b->val();
            cc = c->val();
            cd = d->val();
            ce = e->val();
            cf = f->val();
            cg = g->val();
            ch = h->val();
        }
        petal2(gv, ca, cb, cc, cd, ce, cf, cg, ch);
        ai.pop();
        ++i;
    }
}

Color Cal2Panel::blend(const Color& c, const Color& c2, float per) {
    float r = (float)c.r, g = (float)c.g, b = (float)c.b;
    float r2 = (float)c2.r, g2 = (float)c2.g, b2 = (float)c2.b;
    int rn = (int)(r * per + (1.0f - per) * r2);
    if (rn < 0) rn = 0;
    if (rn > 255) rn = 255;
    int gn = (int)(g * per + (1.0f - per) * g2);
    if (gn < 0) gn = 0;
    if (gn > 255) gn = 255;
    int bn = (int)(b * per + (1.0f - per) * b2);
    if (bn < 0) bn = 0;
    if (bn > 255) bn = 255;
    return Color(rn, gn, bn);
}

void Cal2Panel::drawdate(Graphics& gv) {
    int W = gv.fb->w, H = gv.fb->h;
    int x = W / 4;
    int y = H / 4;
    Date d = Clock::date();
    curm = d.m;
    curd = d.d;

    std::string s = std::to_string(curm);
    for (size_t i = 0; i < s.size(); ++i) {
        int idx = s[i] - '0';
        gv.drawImage(*bims[idx], x, y);
        x += bimws[idx] + 1;
    }

    gv.setColor(Color::white());
    x += 8;
    gv.drawLine(x - 5, y - bimh / 4, x - 5, y + bimh + bimh / 4);

    s = std::to_string(curd);
    for (size_t i = 0; i < s.size(); ++i) {
        int idx = s[i] - '0';
        gv.drawImage(*rims[idx], x, y);
        x += rimws[idx] + 1;
    }
}

bool Cal2Panel::mouseUp(int x, int y) {
    (void)x;
    (void)y;
    startmorph = Clock::now();
    morph();
    return true;
}

void Cal2Panel::redraw(Graphics& gv) {
    int W = gv.fb->w, H = gv.fb->h;
    long long t = Clock::now();

    gv.setColor(Color::black());
    gv.fillRect(0, 0, W, H);
    gv.drawImage(*shisim, 8, H - shisimh - 8);
    gv.drawImage(*maedaim, W - maedaimw - 1, 0);
    drawdate(gv);

    ai.push();
    ai.translate((float)(W / 2), (float)(H / 2));
    int cx = W / 2;
    int cy = H / 2;
    long long dist = (long long)(mx - cx) * (mx - cx) + (long long)(my - cy) * (my - cy);
    float sc = 0.1f + (float)std::sqrt((double)dist) / (float)W;
    float maxsc = 0.6f;
    if (sc > maxsc) sc = maxsc;
    ai.scale(sc, sc);
    petal(gv);
    ai.pop();

    gv.setColor(Color::darkGray());
    gv.drawRect(0, 0, W - 1, H - 1);

    drawtime = Clock::now() - t;
}
