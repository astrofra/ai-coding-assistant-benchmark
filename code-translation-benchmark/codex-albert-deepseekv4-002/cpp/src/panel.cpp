#include "panel.h"

#include <cmath>
#include <cstdio>
#include <string>

#include "clock.h"

namespace flora {

namespace {

const Color kBlack(0, 0, 0);
const Color kDarkGray(64, 64, 64);
const Color kWhite(255, 255, 255);
const Color kPink(255, 175, 175);
const Color kBlue(0, 0, 255);
const Color kOrange(255, 200, 0);
const Color kRed(255, 0, 0);
const Color kWhiteFlower(255, 255, 255);
const Color kGreen(0, 255, 0);
const Color kYellow(255, 255, 0);

}  // namespace

Panel::Panel(const Sprite& shisim, const Sprite& maedaim,
             const std::array<Sprite, 10>& rims,
             const std::array<Sprite, 10>& bims)
    : a_(gdur_),
      b_(gdur_),
      c_(gdur_),
      d_(gdur_),
      g_(gdur_),
      e_(gdur_),
      f_(gdur_),
      h_(gdur_),
      colr_(gdur_),
      colg_(gdur_),
      colb_(gdur_),
      shisim_(shisim),
      maedaim_(maedaim),
      rims_(rims),
      bims_(bims) {
    for (int i = 0; i < 10; i++) {
        rimws_[i] = rims[i].w;
        bimws_[i] = bims[i].w;
    }
    rimh_ = rims[0].h;
    bimh_ = bims[0].h;
    doinits();
}

void Panel::mvector(float ca, float cb, float cc, float cd,
                    int ce, int cf, float cg, int ch, const Color& ccol) {
    a_.next(ca);
    b_.next(cb);
    c_.next(cc);
    d_.next(cd);
    e_.next(ce);
    f_.next(cf);
    g_.next(cg);
    h_.next(ch);
    colr_.next(ccol.r);
    colg_.next(ccol.g);
    colb_.next(ccol.b);
}

void Panel::addval(float ca, float cb, float cc, float cd,
                   int ce, int cf, float cg, int ch, const Color& ccol) {
    if (!init_done_) {
        init_done_ = true;
        // Matches the Java first-allocation quirk: preset 1 pushed twice.
        mvector(ca, cb, cc, cd, ce, cf, cg, ch, ccol);
        mvector(ca, cb, cc, cd, ce, cf, cg, ch, ccol);
    }
    as_[nmorphs_] = ca;
    bs_[nmorphs_] = cb;
    cs_[nmorphs_] = cc;
    ds_[nmorphs_] = cd;
    gs_[nmorphs_] = cg;
    hs_[nmorphs_] = ch;
    es_[nmorphs_] = ce;
    fs_[nmorphs_] = cf;
    cols_[nmorphs_] = ccol;
    ++nmorphs_;
}

void Panel::morph() {
    ++morphc_;
    if (morphc_ >= nmorphs_) morphc_ = 0;
    mvector(as_[morphc_], bs_[morphc_], cs_[morphc_], ds_[morphc_],
            es_[morphc_], fs_[morphc_], gs_[morphc_], hs_[morphc_],
            cols_[morphc_]);
}

void Panel::mouseUp() { morph(); }

void Panel::petal2(Surface& gv, float loarm, float arma, float armb,
                   float rot, int x, int y, float hiarm, int hiy,
                   const Color& col) {
    float ax = (float)x - (float)((double)arma * std::cos((double)rot));
    float ay = (float)y - (float)((double)arma * std::sin((double)rot));
    float bx = (float)x + (float)((double)armb * std::cos((double)rot));
    float by = (float)y + (float)((double)armb * std::sin((double)rot));
    ai_.moveto(0.0f, 0.0f);
    ai_.curveto(loarm, 0.0f, ax, ay, (float)x, (float)y);
    ai_.curveto(bx, by, hiarm, (float)hiy, 0.0f, (float)hiy);
    ai_.curveto(-hiarm, (float)hiy, -bx, by, -(float)x, (float)y);
    ai_.curveto(-ax, ay, -loarm, 0.0f, 0.0f, 0.0f);
    ai_.fill(&gv, col);
}

void Panel::petal(Surface& gv) {
    float ca = 0.0f, cb = 0.0f, cc = 0.0f, cd = 0.0f, cg = 0.0f;
    int ce = 0, cf = 0, ch = 0;
    long long now = Clock::now();
    float rotval = (float)(now % 10000L) * 360.0f / 10000.0f;
    Color col(colr_.val(), colg_.val(), colb_.val());
    int n = npetals_;
    for (int i = 1; i < n; i++) {
        ai_.push();
        ai_.rotate((float)(i * 360 / n) + rotval);
        Color pcol = blend(col, kBlack, (float)i / (float)(n - 1));
        if (i == 1) {
            ca = a_.val();
            cb = b_.val();
            cc = c_.val();
            cd = d_.val();
            ce = e_.val();
            cf = f_.val();
            cg = g_.val();
            ch = h_.val();
            petal2(gv, ca, cb, cc, cd, ce, cf, cg, ch, pcol);
        } else {
            petal2(gv, ca, cb, cc, cd, ce, cf, cg, ch, pcol);
        }
        ai_.pop();
    }
}

Color Panel::blend(const Color& c, const Color& c2, float per) {
    float r = (float)c.r;
    float g = (float)c.g;
    float b = (float)c.b;
    float r2 = (float)c2.r;
    float g2 = (float)c2.g;
    float b2 = (float)c2.b;
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

void Panel::drawdate(Surface& gv) {
    int x = width / 4;
    int y = height / 4;
    Date d = Clock::date();
    std::string s = std::to_string(d.month);
    for (size_t i = 0; i < s.size(); i++) {
        int digit = s[i] - '0';
        gv.blit(bims_[digit], x, y);
        x += bimws_[digit] + 1;
    }
    x += 8;
    gv.drawLine(x - 5, y - bimh_ / 4, x - 5, y + bimh_ + bimh_ / 4, kWhite);
    s = std::to_string(d.day);
    for (size_t i = 0; i < s.size(); i++) {
        int digit = s[i] - '0';
        gv.blit(rims_[digit], x, y);
        x += rimws_[digit] + 1;
    }
}

void Panel::doinits() {
    addval(15.5242f, 208.079f, 48.3322f, 2.13347f, 84, 295, 13.0f, 257, kPink);
    addval(35.3553f, 76.0592f, 94.6256f, 2.52134f, 111, 202, 80.1561f, 320,
           Color(104, 41, 94));
    addval(30.2655f, 72.2772f, 125.575f, -3.84251f, 104, 305, 58.0086f, 171,
           kBlue);
    addval(43.0f, 83.1865f, 107.517f, 0.321751f, 61, 116, 140.688f, 310,
           kOrange);
    addval(12.0416f, 160.178f, 64.2806f, 2.64723f, 110, 244, 9.21954f, 218,
           kRed);
    addval(90.7965f, 152.856f, 15.8114f, 0.566227f, 164, 249, 114.983f, 332,
           kWhiteFlower);
    addval(30.0666f, 260.864f, 60.1664f, 0.855933f, 144, 288, 15.2643f, 147,
           kGreen);
    addval(20.6155f, 50.04f, 54.0833f, 1.5153f, 58, 139, 47.8853f, 348,
           kYellow);
}

void Panel::redraw(Surface& surf) {
    surf.clear(kBlack);
    surf.blit(shisim_, 8, height - shisim_.h - 8);
    surf.blit(maedaim_, width - maedaim_.w - 1, 0);
    drawdate(surf);

    ai_.push();
    ai_.translate((float)(width / 2), (float)(height / 2));
    int cx = width / 2;
    int cy = height / 2;
    long long dist = (long long)(mx - cx) * (mx - cx) +
                     (long long)(my - cy) * (my - cy);
    float sc = 0.1f + (float)std::sqrt((double)dist) / (float)width;
    float maxsc = 0.6f;
    if (sc > maxsc) sc = maxsc;
    ai_.scale(sc, sc);
    petal(surf);
    ai_.pop();

    surf.drawRect(0, 0, width - 1, height - 1, kDarkGray);
}

}  // namespace flora
