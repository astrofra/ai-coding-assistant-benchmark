// SDL host: CLI dispatch, windowed animation loop, smoke test, fixtures.
#include "cal2.hpp"
#include <SDL.h>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
void check(bool ok, const char* operation) {
    if (!ok) throw std::runtime_error(std::string(operation) + ": " + SDL_GetError());
}
struct Sdl {
    Sdl() { SDL_SetMainReady(); check(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0,"SDL_Init"); }
    ~Sdl() { SDL_Quit(); }
    Sdl(const Sdl&) = delete;
    Sdl& operator=(const Sdl&) = delete;
};
struct Options {
    int frames = 0, scale = 1;
    bool smoke = false, fixedDate = false;
    host::Date date = {1,1};
};
int positive(const std::string& value, int maximum) {
    std::size_t used = 0;
    int result = std::stoi(value,&used);
    if (used != value.size() || result < 1 || result > maximum)
        throw std::runtime_error("Invalid positive integer: " + value);
    return result;
}
struct Input {
    bool running = true, pressed = false;
    void handle(const SDL_Event& event, cal2panel& calendar) {
        switch (event.type) {
        case SDL_QUIT:
            running = false;
            break;
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_ESCAPE) running = false;
            break;
        case SDL_MOUSEMOTION:
            calendar.handleEvent({event.motion.state ? 506 : 503, event.motion.x, event.motion.y});
            break;
        case SDL_MOUSEBUTTONDOWN:
            pressed = true;
            calendar.handleEvent({501, event.button.x, event.button.y});
            break;
        case SDL_MOUSEBUTTONUP:
            pressed = false;
            calendar.handleEvent({502, event.button.x, event.button.y});
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_CLOSE) running = false;
            if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) { pressed = false; calendar.mbutton = false; }
            if (event.window.event == SDL_WINDOWEVENT_HIDDEN || event.window.event == SDL_WINDOWEVENT_MINIMIZED) calendar.hide();
            if (event.window.event == SDL_WINDOWEVENT_SHOWN || event.window.event == SDL_WINDOWEVENT_RESTORED) calendar.show();
            break;
        default: break;
        }
    }
};

int run(const Options& options) {
    Sdl sdl;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
    std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)> window(
        SDL_CreateWindow("John Maeda - Flora Calendar (C++11/SDL2 port)",
                         SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         400*options.scale,400*options.scale,
                         SDL_WINDOW_ALLOW_HIGHDPI|SDL_WINDOW_RESIZABLE),
        SDL_DestroyWindow);
    check(bool(window),"SDL_CreateWindow");
    std::unique_ptr<SDL_Renderer,decltype(&SDL_DestroyRenderer)> renderer(
        SDL_CreateRenderer(window.get(),-1,SDL_RENDERER_ACCELERATED),SDL_DestroyRenderer);
    if (!renderer) renderer.reset(SDL_CreateRenderer(window.get(),-1,SDL_RENDERER_SOFTWARE));
    check(bool(renderer),"SDL_CreateRenderer");
    check(SDL_RenderSetLogicalSize(renderer.get(),400,400) == 0,"SDL_RenderSetLogicalSize");
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> texture(
        SDL_CreateTexture(renderer.get(),SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,400,400),
        SDL_DestroyTexture);
    check(bool(texture),"SDL_CreateTexture");
    check(SDL_SetTextureBlendMode(texture.get(),SDL_BLENDMODE_NONE) == 0,"SDL_SetTextureBlendMode");
    cal2 app;
    app.init();
    cal2panel& calendar = *app.panel;
    Input input;
    host::Frame frame;
    host::Graphics graphics(frame);
    if (options.fixedDate) host::useFixedDate(options.date);
    int frames = 0;
    while (input.running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) input.handle(event,calendar);
        if (!input.running) break;
        calendar.run(graphics);
        check(SDL_UpdateTexture(texture.get(),nullptr,frame.data(),400*sizeof(std::uint32_t)) == 0,"SDL_UpdateTexture");
        check(SDL_SetRenderDrawColor(renderer.get(),0,0,0,255) == 0,"SDL_SetRenderDrawColor");
        check(SDL_RenderClear(renderer.get()) == 0,"SDL_RenderClear");
        check(SDL_RenderCopy(renderer.get(),texture.get(),nullptr,nullptr) == 0,"SDL_RenderCopy");
        SDL_RenderPresent(renderer.get());
        ++frames;
        if (options.smoke && frames >= 100) break;
        if (options.frames && frames >= options.frames) break;
        SDL_Delay(static_cast<Uint32>(calendar.sleep));
    }
    calendar.stop();
    if (options.smoke) {
        if (frames != 100) throw std::runtime_error("Smoke test ended unexpectedly");
        std::cout << "flora smoke test passed: " << frames << " frames rendered.\n";
    }
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    try {
        Options options;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help") {
                std::cout << "John Maeda - Flora Calendar (C++11/SDL2 port)\n"
                          << "Move the mouse to change the petal size; release a mouse button to morph. Esc quits.\n"
                          << "--scale 1..4  --date YYYY-MM-DD  --frames N\n"
                          << "--smoke-test  --fixtures SCENARIO EXISTING_OUTPUT_DIRECTORY\n";
                return 0;
            } else if (arg == "--fixtures" && argc == 4) {
                host::fixtures(argv[2],argv[3]); return 0;
            } else if (arg == "--smoke-test") options.smoke = true;
            else if ((arg == "--scale" || arg == "--date" || arg == "--frames") && i+1 < argc) {
                const std::string value = argv[++i];
                if (arg == "--scale") options.scale = positive(value,4);
                if (arg == "--frames") options.frames = positive(value,10000000);
                if (arg == "--date") { options.date = host::parseDate(value); options.fixedDate = true; }
            } else throw std::runtime_error("Unknown or incomplete option: " + arg + " (see --help)");
        }
        if (options.smoke && options.frames) throw std::runtime_error("Use --smoke-test without --frames");
        return run(options);
    } catch (const std::exception& error) {
        std::cerr << "flora: " << error.what() << '\n';
        return 1;
    }
}
