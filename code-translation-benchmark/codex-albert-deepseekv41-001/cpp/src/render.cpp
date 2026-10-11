#include "render.h"

#include <algorithm>
#include <cmath>

void Graphics::fillRect(int x, int y, int w, int h) {
    int x0 = std::max(0, x);
    int y0 = std::max(0, y);
    int x1 = std::min(fb->w, x + w);
    int y1 = std::min(fb->h, y + h);
    for (int yy = y0; yy < y1; ++yy) {
        uint32_t* row = &fb->px[(size_t)yy * fb->w];
        for (int xx = x0; xx < x1; ++xx) row[xx] = color;
    }
}

void Graphics::drawImage(const Image& im, int x, int y) {
    for (int sy = 0; sy < im.h; ++sy) {
        int dy = y + sy;
        if (dy < 0 || dy >= fb->h) continue;
        uint32_t* row = &fb->px[(size_t)dy * fb->w];
        const uint32_t* srow = &im.px[(size_t)sy * im.w];
        for (int sx = 0; sx < im.w; ++sx) {
            int dx = x + sx;
            if (dx < 0 || dx >= fb->w) continue;
            row[dx] = 0xFF000000u | (srow[sx] & 0xffffffu);
        }
    }
}

// ---------------------------------------------------------------------------
// Java Graphics.fillPolygon.
//
// For an offscreen TYPE_INT_RGB image this goes through OpenJDK's software
// pipeline (sun.java2d.pipe.LoopPipe -> native ShapeSpanIterator).  We port
// that rasteriser byte-for-byte:
//   * every vertex is shifted by +0.25 px (the "adjust" normalisation),
//   * edges carry an integer span-start plus a 31-bit sub-pixel error,
//   * x advances per scanline with integer accumulate (bumpx / bumperr),
//   * horizontal edges are dropped, and spans are emitted in
//     even-odd order as [x0, x1) on each pixel row.
// ---------------------------------------------------------------------------
namespace {

const int kErrStepMax = 0x7fffffff;
const int OUT_XLO = 1, OUT_XHI = 2, OUT_YLO = 4, OUT_YHI = 8;

struct Seg {
    int curx, cury, lasty;
    int error, bumpx, bumperr;
};

struct SpanFiller {
    Graphics* g;
    int lox, loy, hix, hiy;
    std::vector<Seg> segs;

    void outcode(float x, float y, int& outc) const {
        if (y <= (float)loy) outc = OUT_YLO;
        else if (y >= (float)hiy) outc = OUT_YHI;
        else outc = 0;
        if (x <= (float)lox) outc |= OUT_XLO;
        else if (x >= (float)hix) outc |= OUT_XHI;
    }

    void appendSegment(float x0, float y0, float x1, float y1) {
        if (y0 > y1) {
            float t = x0; x0 = x1; x1 = t;
            t = y0; y0 = y1; y1 = t;
        }
        int istarty = (int)std::ceil((double)(y0 - 0.5f));
        int ilasty = (int)std::ceil((double)(y1 - 0.5f));
        if (istarty >= ilasty || istarty >= hiy || ilasty <= loy) return;

        float dx = x1 - x0;
        float dy = y1 - y0;
        float slope = dx / dy;
        float ystartbump = (float)istarty + 0.5f - y0;
        x0 += ystartbump * dx / dy;

        int istartx = (int)std::ceil((double)(x0 - 0.5f));
        int bumpx = (int)std::floor((double)slope);
        int bumperr = (int)(((double)slope - std::floor((double)slope)) * (double)kErrStepMax);
        int error = (int)((double)(x0 - (float)(istartx - 0.5f)) * (double)kErrStepMax);

        Seg s;
        s.curx = istartx;
        s.cury = istarty;
        s.lasty = ilasty;
        s.error = error;
        s.bumpx = bumpx;
        s.bumperr = bumperr;
        segs.push_back(s);
    }

    // subdivideLine()'s trivial rejection, then straight to appendSegment().
    void subdivideLine(float x0, float y0, float x1, float y1) {
        float minx = x0 < x1 ? x0 : x1, maxx = x0 < x1 ? x1 : x0;
        float miny = y0 < y1 ? y0 : y1, maxy = y0 < y1 ? y1 : y0;
        if (maxy <= (float)loy || miny >= (float)hiy || minx >= (float)hix) return;
        if (maxx <= (float)lox) {
            appendSegment(maxx, y0, maxx, y1);
            return;
        }
        appendSegment(x0, y0, x1, y1);
    }

    void run(const int* xs, const int* ys, int n) {
        if (n <= 0) return;
        const float xoff = 0.25f, yoff = 0.25f;

        float movx = (float)xs[0] + xoff;
        float movy = (float)ys[0] + yoff;
        float curx = movx, cury = movy;
        int outc0;
        outcode(movx, movy, outc0);

        for (int i = 1; i < n; ++i) {
            float x = (float)xs[i] + xoff;
            float y = (float)ys[i] + yoff;
            if (y == cury) {  // horizontal segment: only track the x change
                if (x != curx) {
                    outcode(x, y, outc0);
                    curx = x;
                }
                continue;
            }
            int outc1;
            outcode(x, y, outc1);
            int outc = outc0 & outc1;
            if (outc == 0) {
                appendSegment(curx, cury, x, y);
            } else if (outc == OUT_XLO) {
                appendSegment((float)lox, cury, (float)lox, y);
            }
            outc0 = outc1;
            curx = x;
            cury = y;
        }

        // HANDLEENDPATH -> HANDLECLOSE
        if (curx != movx || cury != movy) subdivideLine(curx, cury, movx, movy);

        int num = (int)segs.size();
        if (num == 0) return;

        std::vector<int> table(num);
        for (int i = 0; i < num; ++i) table[i] = i;
        std::sort(table.begin(), table.end(), [this](int a, int b) {
            const Seg& s1 = segs[a];
            const Seg& s2 = segs[b];
            if (s1.cury != s2.cury) return s1.cury < s2.cury;
            if (s1.curx != s2.curx) return s1.curx < s2.curx;
            return s1.lasty < s2.lasty;
        });

        // initSegmentTable: skip segments already ended at/above loy, then loy--.
        int lo = 0;
        while (lo < num && segs[table[lo]].lasty <= loy) ++lo;
        int cur = lo, hi = lo, y = loy - 1;

        // ShapeSINextSpan, with each produced span filled immediately.
        while (lo < num) {
            if (cur < hi) {
                int x0 = segs[table[cur]].curx;
                if (x0 >= hix) { cur = hi; continue; }
                if (x0 < lox) x0 = lox;
                cur += 2;  // even-odd: pair with the next active segment
                int x1 = (cur <= hi) ? segs[table[cur - 1]].curx : hix;
                if (x1 > hix) x1 = hix;
                if (x1 <= x0) continue;
                if (y >= 0 && y < g->fb->h) {
                    uint32_t* row = &g->fb->px[(size_t)y * g->fb->w];
                    for (int px = x0; px < x1; ++px) row[px] = g->color;
                }
                continue;
            }
            if (++y >= hiy) { lo = cur = hi = num; break; }

            // Throw away segments that end on this scanline.
            cur = hi;
            int newi = hi;
            while (--cur >= lo) {
                int si = table[cur];
                if (segs[si].lasty > y) table[--newi] = si;
            }
            lo = newi;
            if (lo == hi && lo < num) {
                int si = table[lo];
                if (y < segs[si].cury) y = segs[si].cury;
            }
            while (hi < num && segs[table[hi]].cury <= y) ++hi;

            for (cur = lo; cur < hi; ++cur) {
                int si = table[cur];
                int x0 = segs[si].curx;
                int y0 = segs[si].cury;
                int err = segs[si].error;
                if (++y0 == y) {
                    x0 += segs[si].bumpx;
                    err += segs[si].bumperr;
                    x0 -= (err >> 31);
                    err &= kErrStepMax;
                } else {
                    long long steps = (long long)y - (y0 - 1);
                    y0 = y;
                    x0 += (int)(steps * segs[si].bumpx);
                    steps = err + (steps * segs[si].bumperr);
                    x0 += (int)(steps >> 31);
                    err = ((int)steps) & kErrStepMax;
                }
                segs[si].curx = x0;
                segs[si].cury = y0;
                segs[si].error = err;

                int ni;
                for (ni = cur; ni > lo; --ni) {
                    if (segs[table[ni - 1]].curx <= x0) break;
                    table[ni] = table[ni - 1];
                }
                table[ni] = si;
            }
            cur = lo;
        }
    }
};

}  // namespace

void Graphics::fillPolygon(const int* xs, const int* ys, int n) {
    if (n < 3) return;
    SpanFiller f;
    f.g = this;
    f.lox = 0;
    f.loy = 0;
    f.hix = fb->w;
    f.hiy = fb->h;
    f.run(xs, ys, n);
}

// java.awt.Graphics.drawPolygon(xs, ys, n): n connected lines closing the loop.
void Graphics::drawPolygon(const int* xs, const int* ys, int n) {
    if (n < 2) return;
    for (int i = 0; i < n; ++i) {
        int j = (i + 1 == n) ? 0 : i + 1;
        drawLine(xs[i], ys[i], xs[j], ys[j]);
    }
}

// General Bresenham line (all call sites are axis-aligned, where this is exact).
void Graphics::drawLine(int x0, int y0, int x1, int y1) {
    int dx = std::abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        putPixel(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// java.awt.Graphics.drawRect(x, y, w, h): outline of a (w+1) x (h+1) rectangle.
void Graphics::drawRect(int x, int y, int w, int h) {
    drawLine(x, y, x + w, y);
    drawLine(x, y + h, x + w, y + h);
    drawLine(x, y, x, y + h);
    drawLine(x + w, y, x + w, y + h);
}
