/*
 * Port of cal2panel: the Flora flower panel. Owns the presets, easing,
 * morph/petal rendering, and the date overlay, drawing into a Surface.
 */
#ifndef FLORA_PANEL_H
#define FLORA_PANEL_H

#include <array>

#include "ctm.h"
#include "easing.h"
#include "surface.h"

namespace flora {

class Panel {
public:
    Panel(const Sprite& shisim, const Sprite& maedaim,
          const std::array<Sprite, 10>& rims,
          const std::array<Sprite, 10>& bims);

    // Simulates a mouse release (click) -> morph advance.
    void mouseUp();

    // Renders one frame at the current clock time into the given surface.
    void redraw(Surface& surf);

    int mx = 0;
    int my = 0;
    int width = 400;
    int height = 400;

private:
    void mvector(float ca, float cb, float cc, float cd,
                 int ce, int cf, float cg, int ch, const Color& ccol);
    void addval(float ca, float cb, float cc, float cd,
                int ce, int cf, float cg, int ch, const Color& ccol);
    void morph();
    void petal2(Surface& gv, float loarm, float arma, float armb,
                float rot, int x, int y, float hiarm, int hiy, const Color& col);
    void petal(Surface& gv);
    static Color blend(const Color& c, const Color& c2, float per);
    void drawdate(Surface& gv);
    void doinits();

    Ctm ai_;
    static const int maxmorphs_ = 14;
    int nmorphs_ = 0;
    int morphc_ = 0;
    std::array<float, maxmorphs_> as_, bs_, cs_, ds_, gs_;
    std::array<int, maxmorphs_> es_, fs_, hs_;
    std::array<Color, maxmorphs_> cols_;
    EFloat a_, b_, c_, d_, g_;
    EInt e_, f_, h_;
    EInt colr_, colg_, colb_;
    static const int gdur_ = 2000;
    static const int npetals_ = 14;

    Sprite shisim_;
    Sprite maedaim_;
    std::array<Sprite, 10> rims_;
    std::array<Sprite, 10> bims_;
    std::array<int, 10> rimws_, bimws_;
    int rimh_ = 0;
    int bimh_ = 0;
    bool init_done_ = false;
};

}  // namespace flora

#endif  // FLORA_PANEL_H
