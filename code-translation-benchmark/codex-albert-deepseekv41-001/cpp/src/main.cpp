// Flora Calendar (John Maeda / Shiseido 1997) — C++11 + SDL2 desktop port.
//
// CLI:
//   flora                         interactive 400x400 window
//   flora --smoke-test            window, exit 0 after 100 rendered frames
//   flora --fixtures SCEN OUT     headless: write PNGs (+ .geometry traces)
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <vector>

#ifdef __APPLE__
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif

#include "assets.h"
#include "clock.h"
#include "panel.h"
#include "render.h"

static const int kWidth = 400;
static const int kHeight = 400;
static const char* kTitle = "John Maeda \xE2\x80\x94 Flora Calendar";

static bool mkdir_p(const std::string& path) {
    std::string cur;
    for (size_t i = 0; i < path.size(); ++i) {
        cur += path[i];
        if (path[i] == '/' || i + 1 == path.size()) {
            std::string d = cur;
            if (!d.empty() && d[d.size() - 1] == '/') d.erase(d.size() - 1);
            if (d.empty()) continue;
            if (mkdir(d.c_str(), 0755) != 0 && errno != EEXIST) return false;
        }
    }
    return true;
}

static Assets load_assets(const std::string& dir) {
    Assets a;
    a.shiseido = load_gif(dir + "/shiseido.gif");
    a.maeda2 = load_gif(dir + "/maeda2.gif");
    for (int i = 0; i < 10; ++i) {
        a.rims[i] = load_gif(dir + "/r" + std::to_string(i) + ".gif");
        a.bims[i] = load_gif(dir + "/b" + std::to_string(i) + ".gif");
    }
    return a;
}

static Framebuffer render_frame(Cal2Panel& p) {
    Framebuffer fb(kWidth, kHeight);
    Graphics g(&fb);
    p.redraw(g);
    return fb;
}

// Mirrors FloraReference.fixtures: an asset atlas first, then one output per
// scenario row (a "-" name renders and discards a frame).
static int run_fixtures(const std::string& scenario, const std::string& output) {
    std::string dir = find_asset_dir();
    Assets assets = load_assets(dir);

    if (!mkdir_p(output)) {
        std::fprintf(stderr, "Cannot create output directory: %s\n", output.c_str());
        return 1;
    }

    // decoded-assets.png (as in the Java reference harness).
    {
        Framebuffer atlas(kWidth, kHeight);
        Graphics g(&atlas);
        for (int i = 0; i < 10; ++i) {
            g.drawImage(assets.bims[i], 10 + 20 * i, 20);
            g.drawImage(assets.rims[i], 10 + 20 * i, 60);
        }
        g.drawImage(assets.maeda2, 10, 100);
        g.drawImage(assets.shiseido, 50, 100);
        write_png((output + "/decoded-assets.png").c_str(), atlas);
    }

    std::ifstream in(scenario.c_str());
    if (!in) {
        std::fprintf(stderr, "Cannot read scenario: %s\n", scenario.c_str());
        return 1;
    }

    Cal2Panel p(assets);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::vector<std::string> f;
        std::string tok;
        while (ss >> tok) f.push_back(tok);
        if (f.empty() || f[0][0] == '#') continue;
        if (f.size() < 7) {
            std::fprintf(stderr, "Malformed scenario row: %s\n", line.c_str());
            return 1;
        }

        Clock::time_ms = std::stoll(f[0]);
        p.mx = std::stoi(f[1]);
        p.my = std::stoi(f[2]);
        int clicks = std::stoi(f[3]);
        for (int i = 0; i < clicks; ++i) p.mouseUp(p.mx, p.my);
        Clock::fixed_date = Date(2024, std::stoi(f[4]), std::stoi(f[5]));
        Clock::has_fixed_date = true;
        const std::string& name = f[6];

        if (name == "-") {
            render_frame(p);
            continue;
        }

        std::ofstream trace((output + "/" + name + ".geometry").c_str());
        Clock::geometry = &trace;
        Framebuffer fb = render_frame(p);
        Clock::geometry = nullptr;
        trace.close();

        if (!write_png((output + "/" + name + ".png").c_str(), fb)) {
            std::fprintf(stderr, "Cannot write %s/%s.png\n", output.c_str(), name.c_str());
            return 1;
        }
    }
    return 0;
}

static int run_window(bool smoke) {
    std::string dir = find_asset_dir();
    Assets assets = load_assets(dir);
    Cal2Panel p(assets);

    bool have_window = false;
    SDL_Window* win = nullptr;
    SDL_Renderer* ren = nullptr;
    SDL_Texture* tex = nullptr;

    if (SDL_Init(SDL_INIT_VIDEO) == 0) {
        win = SDL_CreateWindow(kTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               kWidth, kHeight, SDL_WINDOW_SHOWN);
        if (win != nullptr) {
            ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
            if (ren == nullptr) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
            if (ren != nullptr) {
                tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
                                        SDL_TEXTUREACCESS_STREAMING, kWidth, kHeight);
                if (tex != nullptr) have_window = true;
            }
        }
        if (have_window) {
            SDL_RaiseWindow(win);  // bring to front on macOS
        } else {
            std::fprintf(stderr, "Warning: could not create SDL window (%s)\n", SDL_GetError());
        }
    } else {
        std::fprintf(stderr, "Warning: SDL_Init failed (%s)\n", SDL_GetError());
    }

    if (!have_window && !smoke) {
        std::fprintf(stderr, "Error: no display available for the interactive window.\n");
        return 1;
    }

    Framebuffer fb(kWidth, kHeight);
    Graphics g(&fb);
    bool running = true;
    int frames = 0;

    while (running) {
        if (have_window) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) {
                    running = false;
                } else if (e.type == SDL_MOUSEMOTION) {
                    p.mx = e.motion.x;
                    p.my = e.motion.y;
                } else if (e.type == SDL_MOUSEBUTTONDOWN) {
                    p.mx = e.button.x;
                    p.my = e.button.y;
                } else if (e.type == SDL_MOUSEBUTTONUP) {
                    p.mx = e.button.x;
                    p.my = e.button.y;
                    p.mouseUp(p.mx, p.my);
                }
            }
        }

        Clock::tick();
        p.redraw(g);

        if (have_window) {
            SDL_UpdateTexture(tex, nullptr, fb.px.data(), kWidth * 4);
            SDL_RenderClear(ren);
            SDL_RenderCopy(ren, tex, nullptr, nullptr);
            SDL_RenderPresent(ren);
        }

        if (smoke && ++frames == 100) running = false;
        SDL_Delay(5);  // javax.swing.Timer(5, ...) pacing
    }

    if (have_window) {
        SDL_DestroyTexture(tex);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
    }
    SDL_Quit();
    return 0;
}

int main(int argc, char** argv) {
    Clock::init();
    try {
        if (argc == 1) {
            return run_window(false);
        }
        if (argc == 2 && std::strcmp(argv[1], "--smoke-test") == 0) {
            return run_window(true);
        }
        if (argc == 4 && std::strcmp(argv[1], "--fixtures") == 0) {
            return run_fixtures(argv[2], argv[3]);
        }
        std::fprintf(stderr, "Usage: [--smoke-test | --fixtures SCENARIO OUTPUT]\n");
        return 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
