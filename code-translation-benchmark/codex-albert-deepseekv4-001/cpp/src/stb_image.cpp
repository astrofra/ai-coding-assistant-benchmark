// Single-compilation-unit for the vendored stb_image.h (GIF decoder).
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_GIF
#define STBI_WINDOWS_UTF8
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#include <stb_image.h>
