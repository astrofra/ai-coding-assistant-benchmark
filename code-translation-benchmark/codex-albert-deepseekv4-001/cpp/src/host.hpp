// Explicit platform substitutes for the AWT services used by the recovered code.
// This file and host.cpp are restoration infrastructure, not Maeda source.
#pragma once
#include "assets.hpp"
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>
namespace host {
const double PI = 3.14159265358979323846;
const int size = 400;
typedef flora::media::Sprite Image;
struct Date { int month, day; };
struct Rectangle { int width, height; };
struct Event { int id, x, y; };
struct Point { int x, y; };
struct Color {
    int r, g, b;
    Color(int r = 0, int g = 0, int b = 0);
    int getRed() const { return r; }
    int getGreen() const { return g; }
    int getBlue() const { return b; }
    std::uint32_t argb() const;
    static const Color black, white, pink, blue, orange, red, green, yellow, darkGray;
};
std::int64_t currentTimeMillis();
Date localDate();
Date parseDate(const std::string& value);
void useFixedTime(std::int64_t time);
void useFixedDate(Date date);
class Frame : public Image {
public:
    Frame();
    void clear();
    void pixel(int x, int y, std::uint32_t color);
    void polygon(const std::vector<Point>& points, std::uint32_t color);
    void sprite(int x, int y, int width, int height, const std::uint32_t* pixels);
    void savePNG(const std::string& path) const;
    const std::uint32_t* data() const { return pixels.data(); }
};
class Graphics {
    Frame* target;
    Color color;
public:
    std::ostream* geometry;
    explicit Graphics(Frame& frame, std::ostream* trace = nullptr) : target(&frame), geometry(trace) {}
    void setColor(Color c) { color = c; }
    void fillRect(int x, int y, int width, int height);
    void drawRect(int x, int y, int width, int height);
    void drawLine(int x1, int y1, int x2, int y2);
    void drawImage(const Image* im, int x, int y, const void* observer = nullptr);
    void fillPolygon(const std::vector<int>& x, const std::vector<int>& y, int n);
    void drawPolygon(const std::vector<int>& x, const std::vector<int>& y, int n);
};
void fixtures(const std::string& scenario, const std::string& directory);
}
