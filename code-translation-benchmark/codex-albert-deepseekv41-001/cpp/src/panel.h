// Port of cal2panel (Flora Calendar #3), including its IdlePanel base.
#pragma once

#include <memory>
#include <string>

#include "ctm.h"
#include "easing.h"
#include "render.h"

// Loaded sprites referenced by the panel (shared, non-owning from the panel's view).
struct Assets {
    Image shiseido;
    Image maeda2;
    Image rims[10];
    Image bims[10];
};

class Cal2Panel {
public:
    // IdlePanel state.
    int mx = 0, my = 0;

    Ctm ai;

    int cury = 0, curd = 0, curm = 0;
    int ndays = 0, curdy = 0, firstdy = 0;

    static const int maxmorphs = 14;
    int nmorphs = 0;
    int morphc = 0;
    int gdur = 2000;
    int npetals = 14;

    Cal2Panel(const Assets& a);

    void mvector(float ca, float cb, float cc, float cd, int ce, int cf, float cg, int ch, const Color& ccol);
    void addval(float ca, float cb, float cc, float cd, int ce, int cf, float cg, int ch, const Color& ccol);
    void morph();
    void doinits();

    void petal2(Graphics& gv, float loarm, float arma, float armb, float rot, int x, int y, float hiarm, int hiy);
    void petal(Graphics& gv);
    Color blend(const Color& c, const Color& c2, float per);
    void drawdate(Graphics& gv);

    bool mouseUp(int x, int y);
    void redraw(Graphics& gv);

    long long drawtime = 0;
    long long startmorph = -1;

private:
    const Image* shisim = nullptr;
    const Image* maedaim = nullptr;
    const Image* rims[10];
    const Image* bims[10];
    int shisimw = 0, shisimh = 0;
    int maedaimw = 0, maedaimh = 0;
    int rimh = 0, bimh = 0;
    int rimws[10];
    int bimws[10];

    bool allocated = false;
    float as[maxmorphs];
    float bs[maxmorphs];
    float cs[maxmorphs];
    float ds[maxmorphs];
    float gs[maxmorphs];
    int hs[maxmorphs];
    int es[maxmorphs];
    int fs[maxmorphs];
    Color cols[maxmorphs];

    std::unique_ptr<Efloat> a, b, c, d, g;
    std::unique_ptr<Eint> e, f, h;
    std::unique_ptr<Eint> colr, colg, colb;
};
