// Translation/adaptation of source/cfr/cal2.java (Applet/MediaTracker replacement).
#include "cal2.hpp"
void cal2::init() {
    const flora::media::Assets& assets = flora::media::images();
    this->shisim = &assets.shiseido;
    this->maedaim = &assets.maeda2;
    int i = 0;
    while (i < 10) {
        this->imsr[i] = &assets.r_digits[i];
        this->imsb[i] = &assets.b_digits[i];
        ++i;
    }
    this->panel.reset(new cal2panel(this->shisim, this->maedaim, this->imsr, this->imsb));
}
bool cal2::handleEvent(host::Event event) {
    return this->panel ? this->panel->handleEvent(event) : false;
}
void cal2::paint(host::Graphics&) {}
