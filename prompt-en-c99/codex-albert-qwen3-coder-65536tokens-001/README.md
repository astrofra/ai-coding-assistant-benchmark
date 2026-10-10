# Raytracer Implementation

This directory contains the implementation of a C99 mini ray tracer as specified in the documentation.

## Files Generated

- `main.c`: Complete ray tracer implementation in a single C99 source file
- `raytracer`: Compiled executable
- `out.tga`: Output image (1280x720 TGA format)

## Implementation Details

- Implements ray tracing with sphere and plane intersections
- Implements both glass and chrome materials with proper reflection/refraction
- Uses three-point lighting with key, fill, and back lights
- Implements procedural checkerboard ground plane
- Implements gradient sky with red-to-orange transition
- Implements distance fog that blends ground into sky
- Uses 2x2 subpixel sampling for anti-aliasing
- Applies gamma correction for proper color representation
- Outputs 24-bit uncompressed TGA format as required

## Compilation and Execution

```bash
cc -std=c99 -pedantic-errors -Wall -Wextra -O2 main.c -lm -o raytracer
./raytracer
```

The raytracer successfully generates the required `out.tga` file with all specified features.
