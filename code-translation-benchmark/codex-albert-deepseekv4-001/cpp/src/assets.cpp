#include "assets.hpp"
#include <SDL.h>
#include <stb_image.h>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

namespace flora { namespace media {
namespace {
std::string assetDirectory() {
    // FLORA_ASSETS overrides; otherwise resolve relative to the executable.
    if (const char* override = std::getenv("FLORA_ASSETS")) {
        std::string dir(override);
        if (!dir.empty() && dir[dir.size()-1] != '/') dir += '/';
        return dir;
    }
    std::unique_ptr<char, void(*)(void*)> base(SDL_GetBasePath(), SDL_free);
    if (!base) throw std::runtime_error(std::string("Cannot locate application assets: ") + SDL_GetError());
    return std::string(base.get()) + "assets/ims/";
}
Sprite decode(const std::string& path) {
    Sprite sprite;
    int components = 0;
    std::unique_ptr<stbi_uc, void(*)(void*)> rgba(
        stbi_load(path.c_str(), &sprite.width, &sprite.height, &components, 4),
        stbi_image_free);
    if (!rgba) throw std::runtime_error("Cannot load GIF " + path + ": " + stbi_failure_reason());
    sprite.pixels.reserve(sprite.width * sprite.height);
    for (int i = 0; i < sprite.width * sprite.height; ++i) {
        const stbi_uc* p = rgba.get() + 4*i;
        sprite.pixels.push_back((std::uint32_t(p[3])<<24) | (std::uint32_t(p[0])<<16) |
                                (std::uint32_t(p[1])<<8) | std::uint32_t(p[2]));
    }
    return sprite;
}
}
Assets::Assets() {
    const std::string directory = assetDirectory();
    shiseido = decode(directory + "shiseido.gif");
    maeda2 = decode(directory + "maeda2.gif");
    for (int i = 0; i < 10; ++i) {
        r_digits[i] = decode(directory + "r" + std::to_string(i) + ".gif");
        b_digits[i] = decode(directory + "b" + std::to_string(i) + ".gif");
    }
}
const Assets& images() { static const Assets assets; return assets; }
} }
