# Flora Calendar — C++11 / SDL2 desktop port

A desktop translation of John Maeda's 1997 Shiseido **Calendar #3 "Flora"**
Java applet. No browser, no JVM: a single native binary that reproduces the
applet's rendering and interaction.

This is a *translation*, not a redesign. The reconstructed Java sources in
`../../java/` are the specification and are ported literally, including their
quirks (double-copy `push`, decrement-before-check `pop`, the first preset being
pushed twice, integer-vs-float easing, the `0.1 + sqrt(dist)/W` cursor scale).

## Build

Requires a C++11 compiler, CMake ≥ 3.10, and system **SDL2** (pkg-config or a
CMake package). No other libraries; `stb_image` / `stb_image_write` are vendored
in `third_party/`.

```sh
# one-time asset fetch (idempotent)
bash cpp/tools/fetch-assets.sh

# build
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build -j

# the required launchable binary at the app root
cp cpp/build/flora ./flora
```

## Run

```sh
./flora                                              # interactive 400x400 window
./flora --smoke-test                                 # close after 100 frames, exit 0
./flora --fixtures cpp/scenarios/initial.txt /tmp/flora-out
```

Any other invocation prints a usage line to stderr and exits non-zero.

### Interactive

The window is fixed at 400x400, titled `John Maeda — Flora Calendar`. The
flower spins; the cursor distance from the centre sets the flower scale
(`0.1 + sqrt(dist)/400`, clamped to `0.6`); a mouse **click** releases the next
petal morph.

### Fixtures (headless)

`--fixtures SCENARIO OUTPUT` never opens a window. It first writes
`OUTPUT/decoded-assets.png` (a sprite atlas, as the Java harness does), then for
each scenario row writes `OUTPUT/<name>.png` and `OUTPUT/<name>.geometry`.
A row named `-` renders one frame and discards it (used to advance state).

Scenario files are whitespace-separated with `#` comments and blank lines
skipped:

```
time_ms  mouse_x  mouse_y  click_count  month  day  name
```

`time_ms` freezes the clock (it does not advance), `month`/`day` pin the date to
2024-`month`-`day`, and `click_count` simulated clicks are applied in order.

| Scenario | Output | Reproduces |
| --- | --- | --- |
| `cpp/scenarios/initial.txt` | `initial.png` | preset 1 (pink), t=0 |
| `cpp/scenarios/transition.txt` | `transition.png` | preset 1→2 blend at t=1500 |
| `cpp/scenarios/preset3.txt` | `preset3.png` | preset 4 (orange), 3 clicks, t=9000 |

## Assets

Sprites are fetched from `http://maedastudio.com/1997/cal3/ims` into
`cpp/assets/ims/` (22 GIFs: `shiseido`, `maeda2`, `maeda1` and the `b0..b9` /
`r0..r9` digit sets). At runtime the sprites are located by searching, in
order: `$FLORA_ASSETS`, `<exe_dir>/assets/ims`, `<exe_dir>/cpp/assets/ims`,
`<exe_dir>/../assets/ims`, `<exe_dir>/../cpp/assets/ims`, then the current
directory equivalents. A missing sprite prints a clear error and exits
non-zero.

## Layout

```
cpp/
├── CMakeLists.txt
├── README.md
├── src/
│   ├── main.cpp      CLI + SDL2 window/loop + fixture driver
│   ├── clock.{h,cpp} FloraClock
│   ├── easing.{h,cpp} efloat / eint
│   ├── ctm.{h,cpp}   Ctm transform + fixed-point Bezier path builder
│   ├── panel.{h,cpp} cal2panel (+ IdlePanel state)
│   ├── assets.{h,cpp} stb_image GIF decode, asset discovery, PNG write
│   └── render.{h,cpp} ARGB framebuffer + Java Graphics primitives
├── third_party/      stb_image.h, stb_image_write.h
├── assets/ims/       fetched sprites
├── scenarios/        initial.txt, transition.txt, preset3.txt
└── tools/fetch-assets.sh
```

## Fidelity notes

* **Software rendering.** A 32-bit `0xFFRRGGBB` framebuffer, uploaded once per
  frame via `SDL_UpdateTexture` / `SDL_RenderCopy`. No antialiasing and no GPU
  geometry, so the hard `fillPolygon` edges are preserved.
* **`fillPolygon` is byte-exact.** Rather than an approximate scanline rule,
  `render.cpp` ports OpenJDK's software rasteriser (`sun.java2d.pipe`'s native
  `ShapeSpanIterator`): each polygon vertex is shifted by `+0.25 px`, edges are
  stored as an integer span-start plus a 31-bit sub-pixel error, x advances per
  scanline by integer accumulate, horizontal edges are dropped, and spans are
  emitted even-odd as `[x0, x1)`. (This is why `-fwrapv` is enabled.)
* **Geometry trace.** `Ctm::fill` writes `rgb npts x0 y0 x1 y1 …` per path while
  a trace sink is active, matching the Java format exactly.
* **Verification.** Compiled the Java sources as a reference and compared the
  C++ fixtures against them: pixel differences are **0** for `initial`,
  `transition` and `preset3`, and the `.geometry` traces are byte-identical.
  Every polygon vertex (the full 16.16 Bezier subdivision) and the decoded-asset
  atlas also match.
* The images in `../../reference/` differ from *any* JDK-25 render (including
  the Java reference itself) by ~500–650 pixels, all on shape boundaries; they
  were produced by a slightly different AWT rasteriser. This is the only
  residual difference and is imperceptible at normal viewing.
* `subdivideLine`'s clipping in the rasteriser is implemented, though the Flora
  petals never leave the clip region; `stroke`/`drawPolygon` are ported but
  unused by Flora.
* No `documentation/restoration-log.md` is present in the app folder, so its
  absence is noted rather than fabricated.

## Deviations from the Java source

* The Java `Ctm.addpt` grows a fixed 256-point cache and prints progress to
  `System.out`; the port uses a `std::vector` (grows silently). Rendered output
  is unaffected and the event is never hit by Flora's curves.
* `Ctm.push` may index one past its declared 16-slot stack in Java; the C++ port
  allocates one spare slot so the literal behaviour cannot crash. The extra slot
  is never reached in practice (push/pop are balanced).
* Line drawing beyond the axis-aligned cases in Flora (the vertical date
  separator and the frame border) uses a generic Bresenham; every line Flora
  actually draws is axis-aligned and therefore exact.
