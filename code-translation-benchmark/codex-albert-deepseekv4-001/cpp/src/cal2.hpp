// Counterpart of source/cfr/cal2.java; applet services live in the SDL host.
#pragma once
#include "cal2panel.hpp"
class cal2 {
public:
    const host::Image* shisim = nullptr;
    const host::Image* maedaim = nullptr;
    std::array<const host::Image*,10> imsr = {}, imsb = {};
    std::unique_ptr<cal2panel> panel;
    void init();
    bool handleEvent(host::Event event);
    void paint(host::Graphics& g);
};
