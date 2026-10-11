#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#include "assets.h"

#include <cstdlib>
#include <stdexcept>
#include <vector>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <sys/stat.h>
#include <unistd.h>
#elif defined(__linux__)
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static bool file_exists(const std::string& p) {
    struct stat st;
    return ::stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

// A directory is usable if it contains the always-present Shiseido logo.
static bool is_asset_dir(const std::string& dir) {
    return file_exists(dir + "/shiseido.gif");
}

static std::string exe_dir() {
#if defined(__APPLE__)
    char buf[4096];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0) return std::string(".");
    char resolved[4096];
    if (realpath(buf, resolved) == nullptr) return std::string(".");
    std::string p(resolved);
    size_t slash = p.find_last_of('/');
    return slash == std::string::npos ? std::string(".") : p.substr(0, slash);
#elif defined(__linux__)
    char buf[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return std::string(".");
    buf[n] = '\0';
    std::string p(buf);
    size_t slash = p.find_last_of('/');
    return slash == std::string::npos ? std::string(".") : p.substr(0, slash);
#else
    return std::string(".");
#endif
}

std::string find_asset_dir() {
    std::vector<std::string> candidates;

    const char* env = std::getenv("FLORA_ASSETS");
    if (env != nullptr && *env != '\0') candidates.push_back(env);

    std::string ed = exe_dir();
    std::string cd = ".";
    candidates.push_back(ed + "/assets/ims");
    candidates.push_back(ed + "/cpp/assets/ims");   // binary at app root, sources in cpp/
    candidates.push_back(ed + "/../assets/ims");    // binary inside build/
    candidates.push_back(ed + "/../cpp/assets/ims");
    candidates.push_back(cd + "/assets/ims");
    candidates.push_back(cd + "/cpp/assets/ims");

    for (size_t i = 0; i < candidates.size(); ++i) {
        if (is_asset_dir(candidates[i])) return candidates[i];
    }

    std::string msg = "Cannot locate image assets (ims/). Tried:\n";
    for (size_t i = 0; i < candidates.size(); ++i) msg += "  " + candidates[i] + "\n";
    msg += "Set FLORA_ASSETS to the ims directory, or run tools/fetch-assets.sh.";
    throw std::runtime_error(msg);
}

Image load_gif(const std::string& path) {
    int w = 0, h = 0, channels = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (data == nullptr) {
        throw std::runtime_error("Invalid or missing asset: " + path);
    }
    Image im;
    im.w = w;
    im.h = h;
    im.px.resize((size_t)w * h);
    for (size_t i = 0; i < im.px.size(); ++i) {
        unsigned char r = data[i * 4 + 0];
        unsigned char g = data[i * 4 + 1];
        unsigned char b = data[i * 4 + 2];
        im.px[i] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }
    stbi_image_free(data);
    return im;
}

bool write_png(const char* path, const Framebuffer& fb) {
    std::vector<unsigned char> rgb((size_t)fb.w * fb.h * 3);
    for (size_t i = 0; i < fb.px.size(); ++i) {
        uint32_t p = fb.px[i];
        rgb[i * 3 + 0] = (unsigned char)((p >> 16) & 0xff);
        rgb[i * 3 + 1] = (unsigned char)((p >> 8) & 0xff);
        rgb[i * 3 + 2] = (unsigned char)(p & 0xff);
    }
    return stbi_write_png(path, fb.w, fb.h, 3, rgb.data(), fb.w * 3) != 0;
}
