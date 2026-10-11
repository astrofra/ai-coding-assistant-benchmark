#include "surface.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace flora {

Surface::Surface(int w, int h) : w_(w), h_(h), pixels_(w * h * 4, 0) {}

void Surface::clear(const Color& c) {
    const std::uint8_t rgb[4] = {(std::uint8_t)c.r, (std::uint8_t)c.g,
                                 (std::uint8_t)c.b, 0xFF};
    std::uint8_t* p = pixels_.data();
    for (size_t i = 0; i < pixels_.size(); i += 4) {
        std::memcpy(p + i, rgb, 4);
    }
}

void Surface::fillRect(int x, int y, int w, int h, const Color& c) {
    int x0 = std::max(0, x);
    int y0 = std::max(0, y);
    int x1 = std::min(w_, x + w);
    int y1 = std::min(h_, y + h);
    if (x1 <= x0 || y1 <= y0) return;
    for (int yy = y0; yy < y1; yy++) {
        std::uint8_t* row = pixels_.data() + ((size_t)yy * w_ + x0) * 4;
        for (int xx = x0; xx < x1; xx++) {
            row[0] = (std::uint8_t)c.r;
            row[1] = (std::uint8_t)c.g;
            row[2] = (std::uint8_t)c.b;
            row[3] = 0xFF;
            row += 4;
        }
    }
}

void Surface::drawRect(int x, int y, int w, int h, const Color& c) {
    fillRect(x, y, w, 1, c);
    fillRect(x, y + h - 1, w, 1, c);
    fillRect(x, y, 1, h, c);
    fillRect(x + w - 1, y, 1, h, c);
}

void Surface::drawLine(int x0, int y0, int x1, int y1, const Color& c) {
    // Only vertical/horizontal lines are used by the panel; branch to the
    // simple axis-aligned case for the vertical date separator.
    if (x0 == x1) {
        int ya = std::min(y0, y1);
        int yb = std::max(y0, y1);
        fillRect(x0, ya, 1, yb - ya + 1, c);
    } else if (y0 == y1) {
        int xa = std::min(x0, x1);
        int xb = std::max(x0, x1);
        fillRect(xa, y0, xb - xa + 1, 1, c);
    } else {
        int steps = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
        for (int s = 0; s <= steps; s++) {
            int x = x0 + (int)(((long)(x1 - x0) * s) / steps);
            int y = y0 + (int)(((long)(y1 - y0) * s) / steps);
            if (x >= 0 && x < w_ && y >= 0 && y < h_)
                fillRect(x, y, 1, 1, c);
        }
    }
}

void Surface::blit(const Sprite& s, int x, int y) {
    int x0 = std::max(0, x);
    int y0 = std::max(0, y);
    int x1 = std::min(w_, x + s.w);
    int y1 = std::min(h_, y + s.h);
    if (x1 <= x0 || y1 <= y0) return;
    for (int yy = y0; yy < y1; yy++) {
        const std::uint8_t* src =
            s.rgba.data() + ((size_t)(yy - y) * s.w + (x0 - x)) * 4;
        std::uint8_t* row = pixels_.data() + ((size_t)yy * w_ + x0) * 4;
        for (int xx = x0; xx < x1; xx++) {
            std::uint8_t a = src[3];
            if (a != 0) {
                // Opaque background elsewhere; source-over onto opaque black.
                row[0] = src[0];
                row[1] = src[1];
                row[2] = src[2];
                row[3] = 0xFF;
            }
            src += 4;
            row += 4;
        }
    }
}

// Even-odd scanline polygon fill using 16.16 fixed point, matching Java's
// hard-edged fillPolygon semantics.
void Surface::fillPolygon(const int* xs, const int* ys, int n, const Color& c) {
    if (n < 3) return;

    struct Edge {
        int xlo, ylo, xhi, yhi;
        int dx, dy;  // dx = xhi-xlo, dy = yhi-ylo (always >= 0)
    };

    Edge edges[256];
    int ne = 0;
    int ymin = 1 << 30;
    int ymax = -(1 << 30);

    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        int x0 = xs[i], y0 = ys[i];
        int x1 = xs[j], y1 = ys[j];
        if (y0 == y1) continue;  // horizontal edge contributes no crossing
        if (y0 < y1) {
            edges[ne] = {x0, y0, x1, y1, x1 - x0, y1 - y0};
        } else {
            edges[ne] = {x1, y1, x0, y0, x0 - x1, y0 - y1};
        }
        if (edges[ne].ylo < ymin) ymin = edges[ne].ylo;
        if (edges[ne].yhi - 1 > ymax) ymax = edges[ne].yhi - 1;
        ++ne;
    }
    if (ne == 0) return;

    int y0 = std::max(0, ymin);
    int y1 = std::min(h_ - 1, ymax);
    if (y1 < y0) return;

    int crossings[256];
    for (int y = y0; y <= y1; y++) {
        int nc = 0;
        for (int e = 0; e < ne; e++) {
            if (y < edges[e].ylo || y >= edges[e].yhi) continue;
            // crossing in 16.16 then rounded to a column
            if (edges[e].dy > 0) {
                long long num = (long long)(y - edges[e].ylo) * edges[e].dx;
                long long q = edges[e].xlo * 65536LL +
                              (num * 65536LL) / edges[e].dy;
                crossings[nc++] = (int)((q + 32768LL) >> 16);
            }
        }
        // Insertion sort the crossings ascending.
        for (int a = 1; a < nc; a++) {
            int key = crossings[a];
            int b = a - 1;
            while (b >= 0 && crossings[b] > key) {
                crossings[b + 1] = crossings[b];
                --b;
            }
            crossings[b + 1] = key;
        }
        for (int k = 0; k + 1 < nc; k += 2) {
            int xa = std::max(0, crossings[k]);
            int xb = std::min(w_ - 1, crossings[k + 1] - 1);
            if (xb < xa) continue;
            std::uint8_t* row = pixels_.data() + ((size_t)y * w_ + xa) * 4;
            for (int x = xa; x <= xb; x++) {
                row[0] = (std::uint8_t)c.r;
                row[1] = (std::uint8_t)c.g;
                row[2] = (std::uint8_t)c.b;
                row[3] = 0xFF;
                row += 4;
            }
        }
    }
}

}  // namespace flora
