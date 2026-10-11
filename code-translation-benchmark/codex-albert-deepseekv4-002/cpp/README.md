# Flora (Maeda Calendar #3) - C++11 / SDL2 Port

A faithful C++11 + SDL2 desktop translation of John Maeda's restored `Flora`
Java applet (Shiseido 1997 calendar, "Flora"). It renders the same 400x400
software framebuffer as the Java offscreen `BufferedImage` with a hard-edged,
non-antialiased polygon fill, reproducing the applet's math (fixed-point Bezier
subdivision, symmetric cubic easing, cyclic morph presets) exactly.

Dependencies: system SDL2 only. `stb_image.h` / `stb_image_write.h` are vendored
under `third_party/` (no SDL2_image, no other libraries).

## One-time asset fetch

The 22 sprite GIFs are not in the repository; fetch them from the original site:

```sh
bash cpp/tools/fetch-assets.sh
```

The script is idempotent (skips files already present) and writes into
`cpp/assets/ims/`. Assets are located at runtime relative to the executable
(`<exe>/assets/ims`, `<exe>/cpp/assets/ims`, ...) or via the `FLORA_ASSETS`
environment variable. A missing sprite aborts with a clear error and non-zero
exit.

## Build

```sh
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build -j
cp cpp/build/flora ./flora      # required launchable binary at app root
```

`CMakeLists.txt` enforces C++11, builds with `-Wall -Wextra -O2`, and links only
SDL2 and `m`.

## Run modes

```sh
# Interactive window (400x400), animates until closed. Mouse position controls
# flower scale; releasing the mouse button advances the morph preset.
./flora

# Smoke test: same but closes automatically after 100 rendered frames, exit 0.
./flora --smoke-test

# Headless fixtures: no window. Per scenario row writes OUTPUT/<name>.png and
# OUTPUT/<name>.geometry (geometry trace, only while a named row renders).
./flora --fixtures cpp/scenarios/INITIAL.txt /tmp/flora-out

# Anything else prints usage to stderr and exits 2.
```

The smoke-test and interactive modes require a real video display. In a headless
environment (no display server) `SDL_Init` reports "the video driver did not add
any displays" and the program exits non-zero; the headless fixture mode works
without any display.

### Scenario format

Whitespace-separated, `#` comments and blank lines skipped:

```
time_ms  mouse_x  mouse_y  click_count  month  day  name
```

Per row the clock is frozen at `time_ms`, the cursor is set, `click_count`
simulated click/morph actions are applied, and the date is pinned to
`2024-month-day`. Rows with `name == "-"` render a frame and discard it; named
rows write `<name>.png` (and `<name>.geometry`). See `cpp/scenarios/*.txt`.

## Fixture workflow

The three reference images under `reference/` map one-to-one to scenarios:

| Reference | Scenario | Notes |
| --- | --- | --- |
| `reference/initial.png` | `cpp/scenarios/initial.txt` | preset 1 (pink) at rest |
| `reference/transition.png` | `cpp/scenarios/transition.txt` | 1 click then captured mid-morph (dt = 500 ms) |
| `reference/preset3.png` | `cpp/scenarios/preset3.txt` | 3 clicks to preset 4 (orange) at max scale |

Parameters were reverse-engineered against the references: all flowers render at
max scale `0.6` (mouse 200 px from center), and the rotation phase equals
`(time_ms % 10000) * 360 / 10000`. Fixture output is deterministic: repeated
runs produce byte-identical PNGs and traces.

### Fidelity note

Fixture PNGs match the reference images except for a small edge-only residual
(about 1.35% of pixels, ~2200/160000) that sits on polygon boundaries. It comes
from the scanline rasterizer versus AWT's `fillPolygon` inclusion rule at shared
edges; interior colors, shape geometry, layout, and animation params are
identical. The `.geometry` traces follow the app's format
`rgb npts x0 y0 x1 y1 ...` with `rgb = color & 0xFFFFFF`.

## Repository layout

```
cpp/
  CMakeLists.txt
  src/            main, clock, easing, ctm, panel, surface, assets
  third_party/    stb_image.h, stb_image_write.h (+ stb_impl.cpp)
  assets/ims/     fetched sprite GIFs
  scenarios/      fixture scenario files
  tools/fetch-assets.sh
  README.md
documentation/maeda-applet-cpp-sdl2-port-brief.md   implementation brief (spec)
flora             launchable binary (build output copy at app root)
```
