/*
 * Flora (Maeda Calendar #3) C++/SDL2 port entry point.
 * CLI: ./flora | ./flora --smoke-test | ./flora --fixtures SCENARIO OUTPUT
 */
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "SDL.h"

#include "assets.h"
#include "clock.h"
#include "panel.h"
#include "surface.h"

#include "stb_image_write.h"

namespace {

const int kWidth = 400;
const int kHeight = 400;
const char* kWindowTitle = "John Maeda - Flora Calendar";

struct LoadedAssets {
    flora::Sprite shiseido;
    flora::Sprite maeda2;
    std::array<flora::Sprite, 10> bims;
    std::array<flora::Sprite, 10> rims;
};

bool loadAll(const std::string& dir, LoadedAssets& out) {
    if (!flora::loadSprite(dir, "shiseido", out.shiseido)) return false;
    if (!flora::loadSprite(dir, "maeda2", out.maeda2)) return false;
    for (int i = 0; i < 10; i++) {
        if (!flora::loadSprite(dir, "b" + std::to_string(i), out.bims[i]))
            return false;
    }
    for (int i = 0; i < 10; i++) {
        if (!flora::loadSprite(dir, "r" + std::to_string(i), out.rims[i]))
            return false;
    }
    return true;
}

bool writePng(const std::string& path, const flora::Surface& surf) {
    const int w = surf.width();
    const int h = surf.height();
    std::vector<unsigned char> rgb((size_t)w * h * 3);
    const unsigned char* p = surf.pixels();
    for (int i = 0; i < w * h; i++) {
        rgb[i * 3 + 0] = p[i * 4 + 0];
        rgb[i * 3 + 1] = p[i * 4 + 1];
        rgb[i * 3 + 2] = p[i * 4 + 2];
    }
    return stbi_write_png(path.c_str(), w, h, 3, rgb.data(), w * 3) != 0;
}

bool makeDirs(const std::string& path) {
    std::string cmd = "mkdir -p '" + path + "'";
    return std::system(cmd.c_str()) == 0;
}

int runFixtures(const std::string& scenarioPath, const std::string& outputDir) {
    std::string dir = flora::findAssetDir();
    if (dir.empty()) {
        std::fprintf(stderr, "flora: cannot locate assets (set FLORA_ASSETS)\n");
        return 1;
    }
    LoadedAssets assets;
    if (!loadAll(dir, assets)) return 1;
    if (!makeDirs(outputDir)) {
        std::fprintf(stderr, "flora: cannot create output dir %s\n",
                     outputDir.c_str());
        return 1;
    }

    flora::Panel panel(assets.shiseido, assets.maeda2, assets.rims, assets.bims);
    flora::Surface surf(kWidth, kHeight);

    std::ifstream in(scenarioPath.c_str());
    if (!in) {
        std::fprintf(stderr, "flora: cannot open scenario %s\n",
                     scenarioPath.c_str());
        return 1;
    }
    std::string line;
    while (std::getline(in, line)) {
        // trim
        size_t b = line.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) continue;
        size_t e = line.find_last_not_of(" \t\r\n");
        std::string t = line.substr(b, e - b + 1);
        if (t.empty() || t[0] == '#') continue;

        std::istringstream ss(t);
        long long time_ms;
        int mouse_x, mouse_y, clicks, month, day;
        std::string name;
        if (!(ss >> time_ms >> mouse_x >> mouse_y >> clicks >> month >> day >>
              name)) {
            std::fprintf(stderr, "flora: bad scenario line: %s\n", t.c_str());
            return 1;
        }

        flora::Clock::setTime(time_ms);
        panel.mx = mouse_x;
        panel.my = mouse_y;
        for (int i = 0; i < clicks; i++) panel.mouseUp();
        flora::Date d;
        d.year = 2024;
        d.month = month;
        d.day = day;
        flora::Clock::setFixedDate(d);

        if (name == "-") {
            panel.redraw(surf);
            continue;
        }

        std::string geomPath = outputDir + "/" + name + ".geometry";
        std::string pngPath = outputDir + "/" + name + ".png";
        std::FILE* trace = std::fopen(geomPath.c_str(), "w");
        if (!trace) {
            std::fprintf(stderr, "flora: cannot open %s\n", geomPath.c_str());
            return 1;
        }
        flora::Clock::geometry = trace;
        panel.redraw(surf);
        flora::Clock::geometry = nullptr;
        std::fclose(trace);
        if (!writePng(pngPath, surf)) {
            std::fprintf(stderr, "flora: cannot write %s\n", pngPath.c_str());
            return 1;
        }
    }
    return 0;
}

int runWindow(bool smoke) {
    std::string dir = flora::findAssetDir();
    if (dir.empty()) {
        std::fprintf(stderr, "flora: cannot locate assets (set FLORA_ASSETS)\n");
        return 1;
    }
    LoadedAssets assets;
    if (!loadAll(dir, assets)) return 1;

    flora::Panel panel(assets.shiseido, assets.maeda2, assets.rims, assets.bims);
    flora::Surface surf(kWidth, kHeight);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "flora: SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        kWindowTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, kWidth,
        kHeight, 0);
    if (!window) {
        std::fprintf(stderr, "flora: SDL_CreateWindow failed: %s\n",
                     SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) {
        std::fprintf(stderr, "flora: SDL_CreateRenderer failed: %s\n",
                     SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    // Use ARGB8888 (rather than RGBA8888): the SDL software renderer used in
    // headless capture mishandles RGBA8888 streaming textures, mapping an
    // opaque black pixel to red. ARGB8888 is the renderer's native format,
    // so the byte reorder below reproduces the exact surface colors.
    SDL_Texture* texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
        kWidth, kHeight);
    if (!texture) {
        std::fprintf(stderr, "flora: SDL_CreateTexture failed: %s\n",
                     SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    int frames = 0;
    bool running = true;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_MOUSEMOTION:
                    panel.mx = ev.motion.x;
                    panel.my = ev.motion.y;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    panel.mx = ev.button.x;
                    panel.my = ev.button.y;
                    break;
                case SDL_MOUSEBUTTONUP:
                    panel.mx = ev.button.x;
                    panel.my = ev.button.y;
                    panel.mouseUp();
                    break;
                default:
                    break;
            }
        }
        flora::Clock::tick();
        panel.redraw(surf);
        // ARGB8888 stores bytes in little-endian [B,G,R,A] order; the surface
        // holds bytes in [R,G,B,A] order, so reorder during the upload.
        void* texPixels = nullptr;
        int texPitch = 0;
        if (SDL_LockTexture(texture, nullptr, &texPixels, &texPitch) == 0) {
            const std::uint8_t* src = surf.pixels();
            unsigned char* dst = static_cast<unsigned char*>(texPixels);
            for (int y = 0; y < kHeight; ++y) {
                unsigned char* row = dst + (size_t)y * texPitch;
                const std::uint8_t* sp = src + (size_t)y * kWidth * 4;
                for (int x = 0; x < kWidth; ++x) {
                    row[x * 4 + 0] = sp[x * 4 + 2];  // B
                    row[x * 4 + 1] = sp[x * 4 + 1];  // G
                    row[x * 4 + 2] = sp[x * 4 + 0];  // R
                    row[x * 4 + 3] = sp[x * 4 + 3];  // A
                }
            }
            SDL_UnlockTexture(texture);
        }
        // Guarantee an opaque black backdrop regardless of the renderer's
        // default draw color, matching the Java applet's black background.
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        ++frames;
        if (smoke && frames >= 100) running = false;
        SDL_Delay(5);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    flora::Clock::init();
    if (argc == 4 && std::strcmp(argv[1], "--fixtures") == 0) {
        return runFixtures(argv[2], argv[3]);
    }
    if (argc == 1 || (argc == 2 && std::strcmp(argv[1], "--smoke-test") == 0)) {
        bool smoke = argc == 2;
        return runWindow(smoke);
    }
    std::fprintf(stderr,
                 "Usage: [--smoke-test | --fixtures SCENARIO OUTPUT]\n");
    return 2;
}
