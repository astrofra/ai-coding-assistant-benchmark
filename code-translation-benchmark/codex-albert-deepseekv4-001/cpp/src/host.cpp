// Restoration infrastructure. Frame scan conversion and PNG output; the shape
// logic lives in the Maeda classes. No Calendar/Path/tween code is reused.
#include "host.hpp"
#include "cal2.hpp"
#include "java_semantics.hpp"
#include <stb_image_write.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <sstream>
#include <stdexcept>
namespace host {
namespace {
bool fixedTime = false, fixedDate = false;
std::int64_t timeOverride = 0;
Date dateOverride = {1,1};
}
const Color Color::black(0,0,0), Color::white(255,255,255), Color::pink(255,175,175),
    Color::blue(0,0,255), Color::orange(255,200,0), Color::red(255,0,0),
    Color::green(0,255,0), Color::yellow(255,255,0), Color::darkGray(64,64,64);
Color::Color(int red, int green, int blue) : r(red), g(green), b(blue) {
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255)
        throw std::out_of_range("Color component outside Java range");
}
std::uint32_t Color::argb() const {
    return 0xff000000u | (std::uint32_t(r)<<16) | (std::uint32_t(g)<<8) | std::uint32_t(b);
}
std::int64_t currentTimeMillis() {
    if (fixedTime) return timeOverride;
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
void useFixedTime(std::int64_t time) { fixedTime = true; timeOverride = time; }
void useFixedDate(Date date) { fixedDate = true; dateOverride = date; }
Date localDate() {
    if (fixedDate) return dateOverride;
    std::time_t now = std::time(nullptr);
    std::tm local = {};
#if defined(_WIN32)
    if (localtime_s(&local, &now) != 0) throw std::runtime_error("Cannot read local date");
#else
    if (!localtime_r(&now, &local)) throw std::runtime_error("Cannot read local date");
#endif
    return {local.tm_mon + 1, local.tm_mday};
}

Date parseDate(const std::string& value) {
    if (value.size() != 10 || value[4] != '-' || value[7] != '-')
        throw std::runtime_error("Date must be YYYY-MM-DD");
    for (std::size_t i = 0; i < value.size(); ++i)
        if (i != 4 && i != 7 && (value[i] < '0' || value[i] > '9'))
            throw std::runtime_error("Date must be YYYY-MM-DD");
    int year = std::stoi(value.substr(0,4)), month = std::stoi(value.substr(5,2));
    int day = std::stoi(value.substr(8,2));
    int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) days[1] = 29;
    if (year < 1 || month < 1 || month > 12 || day < 1 || day > days[month-1])
        throw std::runtime_error("Invalid calendar date");
    return {month, day};
}

Frame::Frame() { width = height = size; pixels.assign(size * size, 0xff000000u); }
void Frame::clear() { std::fill(pixels.begin(), pixels.end(), 0xff000000u); }
void Frame::pixel(int x, int y, std::uint32_t color) {
    if (x >= 0 && y >= 0 && x < width && y < height) pixels[y * width + x] = color;
}
void Frame::sprite(int x, int y, int width, int height, const std::uint32_t* spixels) {
    // Archived GIFs are all opaque; STB preserves their real alpha.
    for (int j = 0; j < height; ++j)
        for (int i = 0; i < width; ++i) pixel(x+i, y+j, spixels[j*width+i]);
}
void Frame::polygon(const std::vector<Point>& points, std::uint32_t color) {
    if (points.size() < 3) return;
    int ymin = height, ymax = -1;
    for (const Point& p : points) { ymin = std::min(ymin, p.y); ymax = std::max(ymax, p.y); }
    std::vector<double> intersections;
    intersections.reserve(points.size());
    for (int y = std::max(0, ymin); y <= std::min(height-1, ymax); ++y) {
        intersections.clear();
        // Integer scanlines match non-antialiased AWT fillPolygon.
        const double sample = y;
        for (std::size_t i = 0, j = points.size()-1; i < points.size(); j = i++) {
            const Point& a = points[j]; const Point& b = points[i];
            if ((a.y <= sample && b.y > sample) || (b.y <= sample && a.y > sample))
                intersections.push_back(a.x + (sample-a.y) * (double(b.x)-a.x) / (double(b.y)-a.y));
        }
        std::sort(intersections.begin(), intersections.end());
        for (std::size_t i = 0; i+1 < intersections.size(); i += 2) {
            const int left = std::max(0, int(std::ceil(intersections[i])));
            const int right = std::min(width, int(std::ceil(intersections[i+1])));
            for (int x = left; x < right; ++x) pixel(x, y, color);
        }
    }
}
void Frame::savePNG(const std::string& path) const {
    // RGB byte order flip from ARGB framebuffer to stbi_write_png.
    std::vector<unsigned char> rgb(size_t(width) * size_t(height) * 3);
    for (std::size_t i = 0, p = 0; i < pixels.size(); ++i, p += 3) {
        rgb[p+0] = (unsigned char)((pixels[i] >> 16) & 255);
        rgb[p+1] = (unsigned char)((pixels[i] >> 8) & 255);
        rgb[p+2] = (unsigned char)(pixels[i] & 255);
    }
    if (!stbi_write_png(path.c_str(), width, height, 3, rgb.data(), width*3))
        throw std::runtime_error("Cannot write image: " + path);
}


void Graphics::fillRect(int x, int y, int width, int height) {
    for (std::int64_t j = std::max<int64_t>(0,y); j < std::min<int64_t>(size,int64_t(y)+height); ++j)
        for (std::int64_t i = std::max<int64_t>(0,x); i < std::min<int64_t>(size,int64_t(x)+width); ++i)
            target->pixel(int(i),int(j),color.argb());
}
void Graphics::drawLine(int x1, int y1, int x2, int y2) {
    // Bresenham is a host substitute, not a reconstruction of AWT rasterization.
    double ax=x1, ay=y1, bx=x2, by=y2, lo=0, hi=1;
    const double dx=bx-ax, dy=by-ay;
    const double p[]={-dx,dx,-dy,dy}, q[]={ax,double(size)-1-ax,ay,double(size)-1-ay};
    for (int i=0;i<4;++i) {
        if (p[i]==0) { if (q[i]<0) return; }
        else if (p[i]<0) lo=std::max(lo,q[i]/p[i]);
        else hi=std::min(hi,q[i]/p[i]);
    }
    if (lo>hi) return;
    x1=int(std::round(ax+lo*dx)); y1=int(std::round(ay+lo*dy));
    x2=int(std::round(ax+hi*dx)); y2=int(std::round(ay+hi*dy));
    int ix=std::abs(x2-x1), sx=x1<x2?1:-1;
    int iy=-std::abs(y2-y1), sy=y1<y2?1:-1, err=ix+iy;
    for (;;) {
        target->pixel(x1,y1,color.argb());
        if (x1==x2 && y1==y2) break;
        const int e2=2*err;
        if (e2>=iy) { err+=iy; x1+=sx; }
        if (e2<=ix) { err+=ix; y1+=sy; }
    }
}
void Graphics::drawRect(int x, int y, int width, int height) {
    drawLine(x,y,x+width,y); drawLine(x+width,y,x+width,y+height);
    drawLine(x+width,y+height,x,y+height); drawLine(x,y+height,x,y);
}
void Graphics::drawImage(const Image* im, int x, int y, const void*) {
    if (im) target->sprite(x,y,im->width,im->height,im->pixels.data());
}
void Graphics::fillPolygon(const std::vector<int>& x, const std::vector<int>& y, int n) {
    std::vector<Point> points;
    if (geometry) *geometry << (color.argb() & 0xffffffu) << ' ' << n;
    for (int i=0;i<n;++i) {
        points.push_back({x.at(i),y.at(i)});
        if (geometry) *geometry << ' ' << x.at(i) << ' ' << y.at(i);
    }
    if (geometry) *geometry << '\n';
    target->polygon(points,color.argb());
}
void Graphics::drawPolygon(const std::vector<int>& x, const std::vector<int>& y, int n) {
    for (int i=0;i<n;++i) {
        int j=(i+1)%n;
        drawLine(x.at(i),y.at(i),x.at(j),y.at(j));
    }
}
void fixtures(const std::string& scenario, const std::string& directory) {
    std::ifstream in(scenario.c_str());
    if (!in) throw std::runtime_error("Cannot open scenarios: " + scenario);
    cal2 app;
    app.init();
    cal2panel& calendar = *app.panel;
    Frame frame;
    std::string line;
    std::int64_t previous = 0;
    while (std::getline(in,line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream row(line);
        std::int64_t time; int x,y,releases; Date date; std::string name;
        if (!(row >> time >> x >> y >> releases >> date.month >> date.day >> name) ||
            time < previous || releases < 0 || releases > 100 ||
            date.month < 1 || date.month > 12 || date.day < 1 || date.day > 31 ||
            name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-_") != std::string::npos)
            throw std::runtime_error("Invalid scenario row: " + line);
        std::string trailing;
        const int days[] = {31,29,31,30,31,30,31,31,30,31,30,31};
        if ((row >> trailing) || date.day > days[date.month-1])
            throw std::runtime_error("Invalid scenario row: " + line);
        previous = time;
        useFixedTime(time);
        useFixedDate(date);
        calendar.handleEvent({503,x,y});
        for (int i = 0; i < releases; ++i) calendar.handleEvent({502,x,y});
        if (name == "-") { Graphics g(frame); calendar.run(g); continue; }
        const std::string base = directory + "/" + name;
        std::ofstream geometry((base+".geometry").c_str());
        if (!geometry) throw std::runtime_error("Cannot write fixtures: " + base);
        Graphics g(frame, &geometry);
        calendar.run(g);
        frame.savePNG(base+".png");
    }
}
}
