/*
 * Asset discovery and GIF decoding via vendored stb_image.h.
 * Assets are located relative to the executable: <exe_dir>/assets/ims,
 * overridable with the FLORA_ASSETS environment variable.
 */
#ifndef FLORA_ASSETS_H
#define FLORA_ASSETS_H

#include <string>

#include "surface.h"

namespace flora {

// Returns the directory holding the sprite GIFs, or empty string on failure.
std::string findAssetDir();

// Loads <dir>/<name>.gif; prints a clear error and returns the Sprite on
// success. Returns false (with message on stderr) on failure.
bool loadSprite(const std::string& dir, const std::string& name, Sprite& out);

}  // namespace flora

#endif  // FLORA_ASSETS_H
