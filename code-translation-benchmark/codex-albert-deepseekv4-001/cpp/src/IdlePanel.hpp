// Counterpart of source/cfr/IdlePanel.java; SDL replaces Panel and Thread.
#pragma once
#include "host.hpp"
class IdlePanel {
public:
    bool engine = false;
    bool visible = true;
    std::int64_t sleep = 5;
    int mx = 0, my = 0;
    bool mbutton = false;
    virtual ~IdlePanel() {}
    void start();
    void stop();
    void run(host::Graphics& g);
    virtual void redraw(host::Graphics& g);
    void hide();
    void show();
    bool handleEvent(host::Event e);
    virtual bool mouseUp(host::Event, int, int) { return false; }
    host::Rectangle bounds() const { return {400, 400}; }
};
