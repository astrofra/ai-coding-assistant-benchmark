/*
 * Software framebuffer (TYPE_INT_RGB-like, opaque) plus scalar rasterizers
 * that reproduce Java's hard-edged, non-antialiased look: fillRect,
 * 1px drawRect, sprite blit, and fillPolygon.
 */
#ifndef FLORA_SURFACE_H
#define FLORA_SURFACE_H

#include <cstdint>
#include <cstddef>
#include <vector>

namespace flora {

struct Color {
    int r;
    int g;
    int b;

    Color() : r(0), g(0), b(0) {}
    Color(int rr, int gg, int bb) : r(rr), g(gg), b(bb) {}

    std::uint32_t rgb() const {
        return ((std::uint32_t)(r & 0xFF) << 16) |
               ((std::uint32_t)(g & 0xFF) << 8) |
               (std::uint32_t)(b & 0xFF);
    }
};

// A decoded sprite: RGBA with explicit alpha.
struct Sprite {
    int w = 0;
    int h = 0;
    std::vector<std::uint8_t> rgba;  // w*h*4
};

class Surface {
public:
    Surface(int w, int h);

    int width() const { return w_; }
    int height() const { return h_; }
    const std::uint8_t* pixels() const { return pixels_.data(); }
    std::uint8_t* pixels() { return pixels_.data(); }

    void clear(const Color& c);
    void fillRect(int x, int y, int w, int h, const Color& c);
    void drawRect(int x, int y, int w, int h, const Color& c);
    void drawLine(int x0, int y0, int x1, int y1, const Color& c);
    void blit(const Sprite& s, int x, int y);
    void fillPolygon(const int* xs, const int* ys, int n, const Color& c);

private:
    int w_;
    int h_;
    std::vector<std::uint8_t> pixels_;  // RGBA, alpha always 0xFF
};

}  // namespace flora

#endif  // FLORA_SURFACE_H
