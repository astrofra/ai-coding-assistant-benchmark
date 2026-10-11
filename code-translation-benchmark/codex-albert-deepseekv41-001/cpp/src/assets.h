// Sprite loading (GIF via vendored stb_image) and asset-directory discovery.
#pragma once

#include <string>

#include "render.h"

// Decode a GIF into an opaque RGB sprite. Throws std::runtime_error on failure.
Image load_gif(const std::string& path);

// Locate the directory holding the ims sprites. Honours FLORA_ASSETS, then
// searches relative to the executable and the current directory.
// Throws std::runtime_error if no directory with shiseido.gif is found.
std::string find_asset_dir();
