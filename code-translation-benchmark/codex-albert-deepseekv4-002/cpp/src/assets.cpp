#include "assets.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

namespace flora {

static bool fileExists(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}

static std::string dirOf(const std::string& exe) {
    std::size_t slash = exe.find_last_of('/');
    return (slash == std::string::npos) ? std::string(".")
                                        : exe.substr(0, slash);
}

std::string findAssetDir() {
    const char* env = std::getenv("FLORA_ASSETS");
    if (env && env[0]) {
        if (fileExists(std::string(env) + "/shiseido.gif")) return env;
        return std::string();
    }

    // Locate assets relative to the executable. The repo keeps sprites under
    // <app-root>/cpp/assets/ims and the launchable binary in <app-root>, so
    // probe both the direct and repo-relative layouts.
    const std::size_t kMax = 4096;
    char buf[kMax];
    std::string exe;
#if defined(__APPLE__)
    std::uint32_t size = (std::uint32_t)kMax;
    if (_NSGetExecutablePath(buf, &size) == 0) {
        exe = buf;
    }
#else
    if (readlink("/proc/self/exe", buf, kMax - 1) >= 0) exe = buf;
#endif

    if (!exe.empty()) {
        std::string dir = dirOf(exe);
        const std::string candidates[] = {
            dir + "/assets/ims",
            dir + "/cpp/assets/ims",
            dir + "/../assets/ims",
        };
        for (const std::string& cand : candidates) {
            if (fileExists(cand + "/shiseido.gif")) return cand;
        }
    }

    // Fall back to the current working directory layouts.
    const std::string cwd[] = {
        "assets/ims",
        "cpp/assets/ims",
    };
    for (const std::string& cand : cwd) {
        if (fileExists(cand + "/shiseido.gif")) return cand;
    }
    return std::string();
}

bool loadSprite(const std::string& dir, const std::string& name, Sprite& out) {
    std::string path = dir + "/" + name + ".gif";
    int w = 0, h = 0, comp = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &comp, 4);
    if (!data) {
        std::fprintf(stderr, "flora: missing or invalid asset: %s\n", path.c_str());
        return false;
    }
    out.w = w;
    out.h = h;
    out.rgba.assign(data, data + (size_t)w * h * 4);
    stbi_image_free(data);
    return true;
}

}  // namespace flora
