#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace flora { namespace media {
struct Sprite {
    int width = 0, height = 0;
    std::vector<std::uint32_t> pixels;
};
struct Assets {
    Sprite shiseido, maeda2;
    std::array<Sprite,10> r_digits, b_digits;
    Assets();
};
const Assets& images();
} }
