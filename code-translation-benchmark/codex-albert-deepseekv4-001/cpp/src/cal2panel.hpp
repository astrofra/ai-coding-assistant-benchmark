// Counterpart of source/cfr/cal2panel.java, including unused recovered fields.
#pragma once
#include "IdlePanel.hpp"
#include "Ctm.hpp"
#include "efloat.hpp"
#include "eint.hpp"
#include <array>
#include <memory>
class cal2panel : public IdlePanel {
public:
    std::array<std::string,12> stmo = {{"JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE", "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"}};
    std::array<int,12> dimo = {{31,28,31,30,31,30,31,31,30,31,30,31}};
    Ctm ai;
    int cury = 0, curd = 0, curm = 0, ndays = 0, curdy = 0, firstdy = 0;
    std::vector<float> as, bs, cs, ds, gs;
    std::vector<host::Color> cols;
    int maxmorphs = 14, nmorphs = 0, morphc = 0;
    std::vector<int> es, fs, hs;
    std::unique_ptr<efloat> a, b, c, d, g;
    std::unique_ptr<eint> e, f, h;
    int gdur = 2000;
    std::unique_ptr<eint> colr, colg, colb;
    int npetals = 14;
    const host::Image* shisim = nullptr;
    int shisimw = 0, shisimh = 0;
    const host::Image* maedaim = nullptr;
    int maedaimw = 0, maedaimh = 0;
    std::array<const host::Image*,10> rims = {}, bims = {};
    int rimh = 0, bimh = 0;
    std::array<int,10> rimws = {}, bimws = {};
    std::unique_ptr<host::Frame> im;
    std::int64_t drawtime = 0, startmorph = -1;
    void mvector(float ca, float cb, float cc, float cd, int ce, int cf, float cg, int ch, host::Color ccol);
    void addval(float ca, float cb, float cc, float cd, int ce, int cf, float cg, int ch, host::Color ccol);
    void morph();
    void petal2(host::Graphics& gv, float loarm, float arma, float armb, float rot, int x, int y, float hiarm, int hiy);
    void petal(host::Graphics& gv);
    host::Color blend(host::Color c, host::Color c2, float per);
    void doinits();
    void drawdate(host::Graphics& gv);
    cal2panel(const host::Image* sim, const host::Image* mim, std::array<const host::Image*,10> rim, std::array<const host::Image*,10> bim);
    bool mouseUp(host::Event ev, int x, int y) override;
    void redraw(host::Graphics& gv) override;
};
