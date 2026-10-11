// Platform adaptation of source/cfr/IdlePanel.java (lifecycle and event IDs).
#include "IdlePanel.hpp"
void IdlePanel::start() { if (!this->engine) this->engine = true; }
void IdlePanel::stop() { this->engine = false; }
void IdlePanel::run(host::Graphics& g) {
    if (this->engine && this->visible) this->redraw(g);
}
void IdlePanel::redraw(host::Graphics&) {}
void IdlePanel::hide() { this->visible = false; }
void IdlePanel::show() { this->visible = true; }
bool IdlePanel::handleEvent(host::Event e) {
    switch (e.id) {
        case 501:
        case 506:
            this->mbutton = true;
            this->mx = e.x;
            this->my = e.y;
            break;
        case 502:
        case 503:
            this->mbutton = false;
            this->mx = e.x;
            this->my = e.y;
            break;
    }
    return e.id == 502 ? this->mouseUp(e, e.x, e.y) : false;
}
