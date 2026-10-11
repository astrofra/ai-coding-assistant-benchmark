// Translation of source/cfr/cal2panel.java.
#include "cal2panel.hpp"
#include "java_semantics.hpp"
#include <cmath>
#include <stdexcept>
using host::Color;
using host::Graphics;
using host::Image;
using host::Rectangle;
using host::Date;
using host::Event;

void cal2panel::mvector(float ca, float cb, float cc, float cd, int ce, int cf, float cg, int ch, Color ccol) {
    this->a->next(ca);
    this->b->next(cb);
    this->c->next(cc);
    this->d->next(cd);
    this->e->next(ce);
    this->f->next(cf);
    this->g->next(cg);
    this->h->next(ch);
    this->colr->next(ccol.getRed());
    this->colg->next(ccol.getGreen());
    this->colb->next(ccol.getBlue());
}

void cal2panel::addval(float ca, float cb, float cc, float cd, int ce, int cf, float cg, int ch, Color ccol) {
    if (this->as.empty()) {
        this->as.resize(this->maxmorphs);
        this->bs.resize(this->maxmorphs);
        this->cs.resize(this->maxmorphs);
        this->ds.resize(this->maxmorphs);
        this->gs.resize(this->maxmorphs);
        this->hs.resize(this->maxmorphs);
        this->es.resize(this->maxmorphs);
        this->fs.resize(this->maxmorphs);
        this->a.reset(new efloat(this->gdur));
        this->b.reset(new efloat(this->gdur));
        this->c.reset(new efloat(this->gdur));
        this->d.reset(new efloat(this->gdur));
        this->g.reset(new efloat(this->gdur));
        this->h.reset(new eint(this->gdur));
        this->e.reset(new eint(this->gdur));
        this->f.reset(new eint(this->gdur));
        this->cols.resize(this->maxmorphs);
        this->colr.reset(new eint(this->gdur));
        this->colg.reset(new eint(this->gdur));
        this->colb.reset(new eint(this->gdur));
        this->mvector(ca, cb, cc, cd, ce, cf, cg, ch, ccol);
        this->mvector(ca, cb, cc, cd, ce, cf, cg, ch, ccol);
    }
    if (this->nmorphs >= this->maxmorphs) throw std::out_of_range("addval: maxmorphs exceeded");
    this->as[this->nmorphs] = ca;
    this->bs[this->nmorphs] = cb;
    this->cs[this->nmorphs] = cc;
    this->ds[this->nmorphs] = cd;
    this->gs[this->nmorphs] = cg;
    this->hs[this->nmorphs] = ch;
    this->es[this->nmorphs] = ce;
    this->fs[this->nmorphs] = cf;
    this->cols[this->nmorphs] = ccol;
    ++this->nmorphs;
}

void cal2panel::morph() {
    ++this->morphc;
    if (this->morphc >= this->nmorphs) this->morphc = 0;
    this->mvector(this->as[this->morphc], this->bs[this->morphc], this->cs[this->morphc], this->ds[this->morphc],
                  this->es[this->morphc], this->fs[this->morphc], this->gs[this->morphc], this->hs[this->morphc],
                  this->cols[this->morphc]);
}

void cal2panel::petal2(Graphics& gv, float loarm, float arma, float armb, float rot, int x, int y, float hiarm, int hiy) {
    float ax = (float)x - (float)((double)arma * std::cos((double)rot));
    float ay = (float)y - (float)((double)arma * std::sin((double)rot));
    float bx = (float)x + (float)((double)armb * std::cos((double)rot));
    float by = (float)y + (float)((double)armb * std::sin((double)rot));
    this->ai.moveto(0.0f, 0.0f);
    this->ai.curveto(loarm, 0.0f, ax, ay, x, y);
    this->ai.curveto(bx, by, hiarm, hiy, 0.0f, hiy);
    this->ai.curveto(-hiarm, hiy, -bx, by, -x, y);
    this->ai.curveto(-ax, ay, -loarm, 0.0f, 0.0f, 0.0f);
    this->ai.fill(gv);
}

void cal2panel::petal(Graphics& gv) {
    float ca = 0.0f, cb = 0.0f, cc = 0.0f, cd = 0.0f, cg = 0.0f;
    int ce = 0, cf = 0, ch = 0;
    std::int64_t t = host::currentTimeMillis();
    float rotval = (float)(t % 10000L) * 360.0f / 10000.0f;
    // Evaluate in Java left-to-right order, including the three clock reads.
    int red = this->colr->val();
    int green = this->colg->val();
    int blue = this->colb->val();
    Color col = Color(red, green, blue);
    int n = this->npetals;
    int i = 1;
    while (i < n) {
        this->ai.push();
        this->ai.rotate((float)(i * 360 / n) + rotval);
        gv.setColor(this->blend(col, Color::black, (float)i / (float)(n - 1)));
        if (i == 1) {
            ca = this->a->val();
            cb = this->b->val();
            cc = this->c->val();
            cd = this->d->val();
            ce = this->e->val();
            cf = this->f->val();
            cg = this->g->val();
            ch = this->h->val();
            this->petal2(gv, ca, cb, cc, cd, ce, cf, cg, ch);
        } else {
            this->petal2(gv, ca, cb, cc, cd, ce, cf, cg, ch);
        }
        this->ai.pop();
        ++i;
    }
}

Color cal2panel::blend(Color c, Color c2, float per) {
    int bn, gn;
    float r = c.getRed(), g = c.getGreen(), b = c.getBlue();
    float r2 = c2.getRed(), g2 = c2.getGreen(), b2 = c2.getBlue();
    int rn = (int)(r * per + (1.0f - per) * r2);
    if (rn < 0) rn = 0;
    if (rn > 255) rn = 255;
    if ((gn = (int)(g * per + (1.0f - per) * g2)) < 0) gn = 0;
    if (gn > 255) gn = 255;
    if ((bn = (int)(b * per + (1.0f - per) * b2)) < 0) bn = 0;
    if (bn > 255) bn = 255;
    return Color(rn, gn, bn);
}

void cal2panel::doinits() {
    host::currentTimeMillis();
    this->addval(15.5242f, 208.079f, 48.3322f, 2.13347f, 84, 295, 13.0f, 257, Color::pink);
    this->addval(35.3553f, 76.0592f, 94.6256f, 2.52134f, 111, 202, 80.1561f, 320, Color(104, 41, 94));
    this->addval(30.2655f, 72.2772f, 125.575f, -3.84251f, 104, 305, 58.0086f, 171, Color::blue);
    this->addval(43.0f, 83.1865f, 107.517f, 0.321751f, 61, 116, 140.688f, 310, Color::orange);
    this->addval(12.0416f, 160.178f, 64.2806f, 2.64723f, 110, 244, 9.21954f, 218, Color::red);
    this->addval(90.7965f, 152.856f, 15.8114f, 0.566227f, 164, 249, 114.983f, 332, Color::white);
    this->addval(30.0666f, 260.864f, 60.1664f, 0.855933f, 144, 288, 15.2643f, 147, Color::green);
    this->addval(20.6155f, 50.04f, 54.0833f, 1.5153f, 58, 139, 47.8853f, 348, Color::yellow);
}

void cal2panel::drawdate(Graphics& gv) {
    char c;
    Rectangle r = this->bounds();
    int x = r.width / 4;
    int y = r.height / 4;
    Date d = host::localDate();
    this->curm = d.month;
    this->curd = d.day;
    std::string s = std::to_string(this->curm);
    int i = 0;
    while (i < static_cast<int>(s.length())) {
        c = s[i];
        gv.drawImage(this->bims[c - 48], x, y, this);
        x += this->bimws[c - 48] + 1;
        ++i;
    }
    gv.setColor(Color::white);
    x += 8;
    gv.drawLine(x - 5, y - this->bimh / 4, x - 5, y + this->bimh + this->bimh / 4);
    s = std::to_string(this->curd);
    i = 0;
    while (i < static_cast<int>(s.length())) {
        c = s[i];
        gv.drawImage(this->rims[c - 48], x, y, this);
        x += this->rimws[c - 48] + 1;
        ++i;
    }
}

cal2panel::cal2panel(const Image* sim, const Image* mim, std::array<const Image*,10> rim, std::array<const Image*,10> bim) {
    this->maedaim = mim;
    this->rims = rim;
    this->bims = bim;
    this->doinits();
    this->start();
    this->shisim = sim;
    this->shisimw = this->shisim->width;
    this->shisimh = this->shisim->height;
    this->maedaimw = this->maedaim->width;
    this->maedaimh = this->maedaim->height;
    int i = 0;
    while (i < 10) {
        this->rimws[i] = this->rims[i]->width;
        this->bimws[i] = this->bims[i]->width;
        ++i;
    }
    i = 0;
    this->rimh = this->rims[i]->height;
    this->bimh = this->bims[i]->height;
}

bool cal2panel::mouseUp(Event /*ev*/, int /*x*/, int /*y*/) {
    this->startmorph = host::currentTimeMillis();
    this->morph();
    return true;
}

void cal2panel::redraw(Graphics& gv) {
    Rectangle r = this->bounds();
    std::int64_t t = host::currentTimeMillis();
    if (r.width > 16 && !this->im) this->im.reset(new host::Frame());
    Graphics g = this->im ? Graphics(*this->im, gv.geometry) : gv;
    g.setColor(Color::black);
    g.fillRect(0, 0, r.width, r.height);
    g.drawImage(this->shisim, 8, r.height - this->shisimh - 8, this);
    g.drawImage(this->maedaim, r.width - this->maedaimw - 1, 0, this);
    this->drawdate(g);
    this->ai.push();
    this->ai.translate(r.width / 2, r.height / 2);
    int cx = r.width / 2;
    int cy = r.height / 2;
    std::int64_t dist = java::add(java::mul(java::sub(this->mx, cx), java::sub(this->mx, cx)),
                                  java::mul(java::sub(this->my, cy), java::sub(this->my, cy)));
    float sc = 0.1f + (float)std::sqrt(dist) / (float)r.width;
    float maxsc = 0.6f;
    if (sc > maxsc) sc = maxsc;
    this->ai.scale(sc, sc);
    this->petal(g);
    this->ai.pop();
    g.setColor(Color::darkGray);
    g.drawRect(0, 0, r.width - 1, r.height - 1);
    if (this->im != nullptr) gv.drawImage(this->im.get(), 0, 0, this);
    this->drawtime = java::sub64(host::currentTimeMillis(), t);
}
