# Flora Calendar - C++11 / SDL2 port

This is a faithful C++11 + SDL2 desktop translation of the restored John Maeda
"Flora" Java calendar applet (Shiseido / Calendar #3, 1997). It reproduces the
rendering of the Java original: a software 32-bit framebuffer, an integer
non-antialiased polygon fill, the affine `Ctm` transform with fixed-point Bezier
subdivision, the `efloat`/`eint` symmetric cubic easing, and the eight morph
presets.

The port is derived from the Java sources under `java/` (`Ctm.java`,
`cal2panel.java`, `IdlePanel.java`, `efloat.java`, `eint.java`,
`FloraClock.java`, `FloraReference.java`) and is validated against
`reference/*.png`.

## Prerequisites

- CMake >= 3.15
- A C++11 compiler (Apple clang or GCC)
- SDL2 (via `pkg-config` or a `find_package(SDL2)` module)
- `curl` (only for the optional asset fetch; sprites are already vendored)

Network is only needed to re-fetch the sprites. Build and run never touch the
network.

## Asset fetch (one-time, optional)

The 22 sprites are already vendored under `cpp/assets/ims/`. To re-fetch from the
original host:

```sh
bash cpp/tools/fetch-assets.sh
```

## Build

```sh
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build -j
```

The executable lands at `cpp/build/bin/flora`, and the sprites are copied to
`cpp/build/bin/assets/ims/` by the build. The required launchable binary is a
copy at the app root:

```sh
cp cpp/build/bin/flora ./flora
```

## Run

```sh
./flora                    # open the 400x400 window and animate
./flora --smoke-test       # render 100 frames, close, exit 0
./flora --help
```

Move the mouse to change the petal size; release a mouse button to advance to
the next morph preset. `Esc` quits.

Assets are located at `<executable_dir>/assets/ims/`. Override the location with
the `FLORA_ASSETS` environment variable (a directory ending in `/ims/`). A
missing sprite prints a clear error and exits non-zero.

## Fixtures

Headless runs (no window) render per scenario row and write
`OUTPUT/<name>.png` (plus `OUTPUT/<name>.geometry` for the filled-path trace):

```sh
mkdir -p /tmp/flora-out
./flora --fixtures cpp/scenarios/initial.txt    /tmp/flora-out
./flora --fixtures cpp/scenarios/transition.txt /tmp/flora-out
./flora --fixtures cpp/scenarios/preset3.txt    /tmp/flora-out
```

Scenario rows are whitespace-separated:

```
time_ms  mouse_x  mouse_y  click_count  month  day  name
```

The clock is frozen at `time_ms` for that row, the cursor is set, `click_count`
release (morph) actions are applied, the date is pinned to `2024-<month>-<day>`,
and one frame is rendered. If `name` is `-` the frame is rendered and discarded;
otherwise it is saved as `OUTPUT/<name>.png` and the geometry trace as
`OUTPUT/<name>.geometry`. Repeated runs are byte-identical.

The three checked-in scenarios reproduce the canonical sequence used to generate
`reference/{initial,transition,preset3}.png`:

| Scenario | reference | Description |
| --- | --- | --- |
| `initial.txt` | `initial.png` | fresh panel, preset 0 (pink) |
| `transition.txt` | `transition.png` | mid-ease of the first morph (preset 0 to 1) |
| `preset3.txt` | `preset3.png` | after three morphs (preset 3, orange) |

## Fidelity notes

- The scanline polygon fill uses integer sample rows with `ceil` on both edges,
  matching Java AWT `fillPolygon` hard edges.
- Java `int`/`long` wrapping, arithmetic shifts, truncation toward zero, and the
  transform-stack `push`/`pop` quirks are preserved literally via
  `src/java_semantics.hpp`.
- GIF sprites are decoded with the vendored `stb_image.h` and written to PNG with
  the vendored `stb_image_write.h`; no SDL2_image is required.

## Port note

Per the brief, the sources reference `documentation/restoration-log.md`; that
document is not present in this benchmark, so it is not reproduced here.
