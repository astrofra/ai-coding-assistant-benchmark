// Software framebuffer and a small subset of java.awt.Graphics semantics.
//
// Everything is drawn into a 32-bit ARGB (0xFFRRGGBB) buffer exactly like the
// Java offscreen BufferedImage of TYPE_INT_RGB; polygons are filled with an
// integer, non-antialiased scanline algorithm so the hard edges match Java's
// Graphics.fillPolygon.
#pragma once

#include <cstdint>
#include <vector>

// Opaque RGB colour. Mirrors java.awt.Color for our purposes (alpha ignored).
struct Color {
    int r, g, b;
    Color() : r(0), g(0), b(0) {}
    Color(int rr, int gg, int bb) : r(rr), g(gg), b(bb) {}
    uint32_t rgb() const {
        return ((uint32_t)(r & 0xff) << 16) | ((uint32_t)(g & 0xff) << 8) | (uint32_t)(b & 0xff);
    }
    static Color black()  { return Color(0, 0, 0); }
    static Color white()  { return Color(255, 255, 255); }
    static Color pink()   { return Color(255, 175, 175); }
    static Color orange() { return Color(255, 200, 0); }
    static Color red()    { return Color(255, 0, 0); }
    static Color green()  { return Color(0, 255, 0); }
    static Color yellow() { return Color(255, 255, 0); }
    static Color blue()   { return Color(0, 0, 255); }
    static Color darkGray(){ return Color(64, 64, 64); }
};

// Decoded, opaque sprite (rows top-to-bottom, XRGB).
struct Image {
    int w = 0;
    int h = 0;
    std::vector<uint32_t> px;
};

// Software framebuffer, pixel format 0xFFRRGGBB.
struct Framebuffer {
    int w = 0;
    int h = 0;
    std::vector<uint32_t> px;

    Framebuffer() {}
    Framebuffer(int width, int height, uint32_t fill = 0xFF000000u)
        : w(width), h(height), px((size_t)width * height, fill) {}
};

// Graphics context bound to a framebuffer, emulating the Java calls we need.
struct Graphics {
    Framebuffer* fb;
    uint32_t color;  // 0xFFRRGGBB

    explicit Graphics(Framebuffer* f) : fb(f), color(0xFF000000u) {}

    void setColor(const Color& c) { color = 0xFF000000u | c.rgb(); }
    Color getColor() const {
        return Color((int)((color >> 16) & 0xff), (int)((color >> 8) & 0xff), (int)(color & 0xff));
    }
    // Java: g.getColor().getRGB() & 0xffffff
    uint32_t getColorRGB() const { return color & 0xffffffu; }

    void fillRect(int x, int y, int w, int h);
    void drawImage(const Image& im, int x, int y);
    void fillPolygon(const int* xs, const int* ys, int n);
    void drawPolygon(const int* xs, const int* ys, int n);
    void drawLine(int x0, int y0, int x1, int y1);
    void drawRect(int x, int y, int w, int h);

private:
    void putPixel(int x, int y) {
        if (x < 0 || y < 0 || x >= fb->w || y >= fb->h) return;
        fb->px[(size_t)y * fb->w + x] = color;
    }
};

// PNG output (RGB, 8-bit). Returns false on failure.
bool write_png(const char* path, const Framebuffer& fb);
