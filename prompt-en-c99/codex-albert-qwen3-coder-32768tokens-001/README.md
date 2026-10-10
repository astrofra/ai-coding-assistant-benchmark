# C99 Raytracer Implementation

This repository contains a complete implementation of a raytracer as specified in the benchmark requirements.

## Implementation Details

- **Language**: Strict ISO C99
- **File**: `main.c` - Single source file implementation
- **Output**: `out.tga` - 1280×720 uncompressed 24-bit TGA image
- **Executable**: `raytracer` - Compiled binary

## Features Implemented

- Two spheres (glass and chrome) positioned side-by-side
- Infinite black-and-white checkerboard ground plane
- Classic three-point lighting setup
- Gradient sky with red-to-orange transition
- Distance fog effect
- Proper ray tracing with reflections, refractions, and shadows
- TGA file output with correct header format

## Compilation and Execution

```bash
cc -std=c99 -pedantic-errors -Wall -Wextra -O2 main.c -lm -o raytracer
./raytracer
```

## Verification

The generated image meets all specifications:
- Correct dimensions (1280×720)
- Correct file size (2,764,818 bytes)
- Proper TGA header
- Accurate rendering of materials (glass with refraction, chrome with reflection)
- Correct lighting and shadows
- Fog effect blending ground into sky
