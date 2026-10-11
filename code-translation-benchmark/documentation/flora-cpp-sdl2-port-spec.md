# Flora Calendar — Java → C++/SDL2 Desktop Port

**Document type:** implementation brief / specification for an autonomous coding
model (DeepSeek V4 class).
**Target:** a turn-key, compilable, runnable C++17 + SDL2 desktop application that
reproduces the rendering of the restored Java reference in `java/`.

---

## 0. Contract in one paragraph

Port the restored Java applet **“Calendar #3 — Flora”** (John Maeda, 1997, for
Shiseido) located in `java/` to a **single native C++17 + SDL2 desktop program**.
The program must build with `cmake` from a clean checkout, run as a launchable
shell binary, render the **same 400×400 animated frame** as the Java reference
(same geometry, same animation math, same colors, same layout), support the same
CLI modes, and be delivered with a prebuilt binary. The rendering must be verified
against the reference images in `reference/`.

---

## 1. Background / why this is a “repair”

- The Java sources are a **restoration**: `Ctm.java`, `cal2panel.java`, `efloat.java`
  and `eint.java` carry the header *“Adapted from the archived CFR 0.152
  reconstruction; see documentation/restoration-log.md.”* They are a line-by-line
  reconstruction of a decompiled 1997 applet (`cal2.class`). Preserve their behavior,
  including idiosyncrasies — do **not** “clean up” the math.
- `web-page/Calendar #3 _Flora_.html` is the original Maeda Studio page; it embeds
  `<applet code="cal2.class" … width="400" height="400">` and states the interaction:
  *“Click to release a new petal form into the space of the spinning flower.”*
- The original image assets are **not** in this repository but are still hosted by
  the original site (see §6).
- The goal is a **desktop version** (no browser, no JVM) that looks and behaves the
  same, delivered ready to run.

---

## 2. Deliverables and repository layout

Create the C++ port inside `code-translation-benchmark/` with this layout:

```
code-translation-benchmark/
├── cpp/
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── main.cpp            # CLI parsing, window loop, fixtures
│   │   ├── flora_clock.{h,cpp} # time/date/geometry-trace singleton
│   │   ├── ctm.{h,cpp}         # affine transform + integer Bézier path builder
│   │   ├── easing.{h,cpp}      # efloat / eint (cubic ease)
│   │   ├── panel.{h,cpp}       # cal2panel: composition, petals, date
│   │   └── assets.{h,cpp}      # image loading (stb_image), asset discovery
│   ├── third_party/
│   │   ├── stb_image.h         # vendored, GIF-capable
│   │   └── stb_image_write.h   # vendored, PNG writer (fixtures)
│   ├── assets/ims/             # required sprites (see §6), fetched via script
│   ├── scenarios/              # fixture scenario files (see §3)
│   ├── tools/fetch-assets.sh   # downloads the 22 sprites
│   └── README.md               # exact build & run instructions
├── documentation/
│   └── flora-cpp-sdl2-port-spec.md   # this file
└── flora                        # REQUIRED: launchable binary (see §8)
```

**Required outputs**

1. A clean `cmake` build that produces an executable (`flora`).
2. A **launchable binary present at `code-translation-benchmark/flora`** (copy or
   symlink of the build output), so `./flora` works straight from the shell.
3. `cpp/assets/ims/` populated with the 22 sprites, and `tools/fetch-assets.sh`
   able to (re)download them.
4. `cpp/README.md` documenting build, run, and the fixture workflow.
5. Fixture PNGs and geometry traces that match `reference/*.png`.

---

## 3. CLI contract (mirror the Java exactly)

The Java entry point (`FloraReference.main`) supports three modes. Reproduce them
verbatim in argument semantics:

| Invocation | Behavior |
| --- | --- |
| `./flora` | Open the 400×400 window and run the animation loop indefinitely. |
| `./flora --smoke-test` | Same as above, but close automatically after **100 rendered frames** and exit `0`. |
| `./flora --fixtures SCENARIO OUTPUT` | Headless: no window; read the scenario file, write `OUTPUT/<name>.png` and `OUTPUT/<name>.geometry` per line. |
| anything else | Print usage to stderr and exit non-zero. |

**Scenario file format** (whitespace-separated, `#` comments and blank lines
skipped), one fixture per line, fields in this order:

```
time_ms  mouse_x  mouse_y  click_count  month  day  name
```

Semantics per row (must match `FloraReference.fixtures`):

1. `FloraClock.time = time_ms` (milliseconds; the clock does **not** advance).
2. `mouse = (mouse_x, mouse_y)`.
3. Call the panel’s `mouseUp`/`morph` handler **`click_count`** times.
4. `fixedDate = (2024, month, day)` (year is always 2024).
5. If `name == "-"`: render one frame and discard it.
6. Else: open `OUTPUT/<name>.geometry`, render while the geometry trace is active,
   and save the frame as `OUTPUT/<name>.png`.

Provide at least three scenarios under `cpp/scenarios/` whose outputs reproduce
`reference/initial.png`, `reference/transition.png` and `reference/preset3.png`
respectively (name them `initial.txt`, `transition.txt`, `preset3.txt`).

---

## 4. Runtime architecture (SDL2)

- **Window:** `400×400`, non-resizable, title `John Maeda — Flora Calendar`.
  Use `SDL_WINDOW_SHOWN`; on macOS ensure the window is foregrounded.
- **Software rendering (recommended and expected):** keep a `400×400` 32-bit ARGB
  software framebuffer, exactly like the Java offscreen `BufferedImage`. Draw the
  frame into it with your own integer scanline polygon fill, then upload it once per
  frame with `SDL_UpdateTexture` + `SDL_RenderCopy`. This preserves Java’s
  **anti-alias-free, hard-edged** fill and makes headless fixture output trivial.
  Do **not** enable SDL anti-aliased primitives or GPU geometry for the petals.
- **Frame loop:** the Java `javax.swing.Timer` fires every **5 ms**. Render one frame
  per tick (≈200 Hz target); use `SDL_Delay` pacing so the loop does not spin hot.
- **Events:** update `(mx,my)` on mouse **move, drag and press**; on mouse **release**
  (and, in fixtures, per synthetic click) invoke the morph handler.
- **Geometry trace:** when a geometry writer is active (fixtures only), each filled
  path emits one line: `rgb npts x0 y0 x1 y1 …` where `rgb = color & 0xFFFFFF`
  (see §5.8). This must be byte-identical in format to the Java output.
- **Headless:** `--fixtures` must never create a window.

**Dependencies:** SDL2 only (available via `pkg-config`, tested version 2.32.x).
Vendor `stb_image.h` (GIF) and `stb_image_write.h` (PNG) so no SDL2_image is needed.

---

## 5. Exact behavioral specification

Port the following faithfully. All values are **exact**; a small deviation will
break visual/trace comparison.

### 5.1 Clock (`FloraClock`)

- `epoch = current wall time (ms)` and `origin = steady clock (ns)` captured once at start.
- `tick()`: `time = epoch + (now_ns - origin) / 1_000_000`.
- `now()` returns `time`.
- `date()` returns `fixedDate` if set, otherwise the real local date.
- `time` starts at `0` until the first `tick()`. In fixtures it is set explicitly and
  never ticks.

### 5.2 Panel state (`cal2panel`)

- `npetals = 14`, `maxmorphs = 14`, `gdur = 2000` ms.
- Month names and days-in-month arrays are unused for rendering; you may keep or drop them.
- `doinits()` registers **8** morph presets (see §5.6) in order.

### 5.3 Per-frame composition (`redraw`), in this exact order

Let `W = H = 400`.

1. Fill the whole surface with **black**.
2. Blit `shiseido.gif` at `(8, H - shiseido_h - 8)`.
3. Blit `maeda2.gif` at `(W - maeda2_w - 1, 0)`.
4. Draw the date (see §5.4).
5. `push()`; `translate(W/2, H/2)`.
6. Compute `dist = (mx - W/2)² + (my - H/2)²`;
   `sc = 0.1 + sqrt(dist)/W`; clamp `sc ≤ 0.6`; `scale(sc, sc)`.
7. Draw the petals (see §5.5).
8. `pop()`.
9. Draw a 1-px border rectangle in **dark gray** `(64,64,64)`: `(0,0)` to `(W-1,H-1)`.
10. Present.

(For `W ≤ 16` the Java draws directly to the window; at 400×400 this branch is
irrelevant — always render offscreen.)

### 5.4 Date (`drawdate`)

- `x = W/4`, `y = H/4`.
- Month = `date().month`, rendered **digit by digit** with the `b*` sprites
  (`b0..b9`); after each digit `x += sprite_width + 1`.
- `x += 8`; then draw a **white** vertical line from `(x-5, y - bimh/4)` to
  `(x-5, y + bimh + bimh/4)` (where `bimh` is the height of the `b0` sprite = 14).
- Day = `date().day`, rendered digit by digit with the `r*` sprites; after each
  digit `x += sprite_width + 1`.

Digit widths are **per-sprite** and must be read from the loaded images (see §6).
Sprites are drawn opaque (their background is black, matching the panel).

### 5.5 Petals (`petal` + `petal2`)

```
rotval = (clock.now() % 10000) * 360.0 / 10000.0        // degrees, 10 s per turn
col    = rgb(current eased color channels)
n      = npetals                                         // 14
for i = 1 .. n-1:                                        // i.e. 1..13 → 13 petals
    push()
    rotate(i * 360 / n + rotval)                         // integer division for i*360/n
    setColor(blend(col, black, i / (n-1)))               // float division
    petal2(loarm, arma, armb, rot, x, y, hiarm, hiy)     // from current morph state
    pop()
```

`petal2(loarm, arma, armb, rot, x, y, hiarm, hiy)` — with `rot` in **radians**
(the current morph `d` value is in radians here), using `double` cos/sin:

```
ax = x - arma*cos(rot);  ay = y - arma*sin(rot)
bx = x + armb*cos(rot);  by = y + armb*sin(rot)
moveto(0, 0)
curveto(loarm, 0,   ax, ay,   x, y)
curveto(bx, by,     hiarm, hiy, 0, hiy)
curveto(-hiarm, hiy, -bx, by, -x, y)
curveto(-ax, ay,    -loarm, 0,   0, 0)
fill()
```

`blend(c, c2, per)`: `out = c*per + (1-per)*c2` per channel, floored to int and
clamped to `[0,255]`; here `c2 = black`.

`mvector` sets the current morph parameters (`a..h` + color) via the easing objects.
Note the Java quirk: inside `petal()`, the `ca..ch` variables are only assigned when
`i == 1`, then reused unchanged for `i > 1`. Preserve.

### 5.6 Morph presets (`doinits`)

Register in this order (fields: `a, b, c, d` floats; `e, f` ints; `g` float; `h` int; color):

| # | a | b | c | d | e | f | g | h | color |
| - | - | - | - | - | - | - | - | - | - |
| 1 | 15.5242 | 208.079 | 48.3322 | 2.13347 | 84 | 295 | 13.0 | 257 | pink `(255,175,175)` |
| 2 | 35.3553 | 76.0592 | 94.6256 | 2.52134 | 111 | 202 | 80.1561 | 320 | `(104,41,94)` |
| 3 | 30.2655 | 72.2772 | 125.575 | -3.84251 | 104 | 305 | 58.0086 | 171 | blue `(0,0,255)` |
| 4 | 43.0 | 83.1865 | 107.517 | 0.321751 | 61 | 116 | 140.688 | 310 | orange `(255,200,0)` |
| 5 | 12.0416 | 160.178 | 64.2806 | 2.64723 | 110 | 244 | 9.21954 | 218 | red `(255,0,0)` |
| 6 | 90.7965 | 152.856 | 15.8114 | 0.566227 | 164 | 249 | 114.983 | 332 | white `(255,255,255)` |
| 7 | 30.0666 | 260.864 | 60.1664 | 0.855933 | 144 | 288 | 15.2643 | 147 | green `(0,255,0)` |
| 8 | 20.6155 | 50.04 | 54.0833 | 1.5153 | 58 | 139 | 47.8853 | 348 | yellow `(255,255,0)` |

**Initialization quirk:** the first `addval` allocates the state and calls `mvector`
**twice** with preset 1 before storing it; subsequent `addval`s store presets 2..8.
After `doinits`, `nmorphs = 8` and the *current* values equal preset 1.

**Morphing:** `morph()` does `morphc = (morphc + 1) % nmorphs`, then `mvector` with
preset `morphc`. `mvector` calls `next()` on every eased channel **and** on the three
color channels. `mouseUp`/click calls `morph()` (and records `startmorph = now()`).

### 5.7 Easing (`efloat` / `eint`)

Both share `dur = gdur = 2000` and these fields: `t = -1`, `a`, `b`, `cx`.

- `next(c)`: `a = cx; b = c; rewind()` (i.e. `t = -1; donep = false; cx = c`).
- `val()`:
  ```
  if (t == -1) { t = now(); return a; }
  nt = now(); dt = nt - t;
  if (dt >= dur) { donep = true; cx = b; return b; }
  dt = dur - dt;
  top = dt*dt*dt;
  bot = dur*dur*dur;
  cx = top*(a-b)/bot + b;      // efloat: float math; eint: integer/long math, truncated
  return cx;
  ```
  Keep `efloat` using `float` and `eint` using **integer** division/truncation, and
  keep the `long` intermediate products (they can exceed 32 bits: `2000³ = 8e9`).

### 5.8 Transform + path builder (`Ctm`)

- `gctm[9]`, stack `ctms[16][9]`, `cp`, `ONEHALF = 32768`.
- Matrix multiply `MultMat3x3` and the helpers exactly as written (translation,
  scale, rotation, skew); rotation/skew use degrees→radians.
- `push()`: copy `gctm` into `ctms[cp]`, `cp++`, copy `gctm` into `ctms[cp]`
  (if `cp >= 16` return). `pop()`: `cp--`; if `cp < 0` return; copy `ctms[cp]` to `gctm`.
  **Preserve this doubled copy** — it is a quirk of the reconstruction.
- `x(fx,fy) = (int)(fx*gctm[0] + fy*gctm[3] + gctm[6] + 0.5f)`,
  `y(fx,fy) = (int)(fx*gctm[1] + fy*gctm[4] + gctm[7] + 0.5f)`.
- Path points are accumulated in `xpts/ypts` (grow by 256, starting at 256, when
  `npts > len - 16`).
- `moveto/lineto/curveto`: `curveto` converts control points to integer screen space,
  then calls `BCurve` with all coordinates **shifted left by 16** (16.16 fixed point),
  then `addpt(e,f)` with the final control point.
- `BCurve(x1,y1,x2,y2,x3,y3,x4,y4)` — integer midpoint subdivision with the flatness
  test `dx + dy > 10` (each `>> 16`), emitting the midpoint `(x1234 + ONEHALF) >> 16`
  and recursing on both halves. Copy the algorithm literally (watch operator
  precedence from the decompiled one-liner).
- `fill()`: if a geometry writer is active, print `(color_rgb & 0xFFFFFF) + " " + npts`
  then ` x y` pairs, then a newline; then scanline-fill the polygon (integer,
  non-antialiased, Java `Graphics.fillPolygon` semantics); reset `npts = 0`.

### 5.9 Colors

Java `Color` constants used: `pink (255,175,175)`, `blue (0,0,255)`,
`orange (255,200,0)`, `red (255,0,0)`, `white (255,255,255)`, `green (0,255,0)`,
`yellow (255,255,0)`. `darkGray = (64,64,64)`.

---

## 6. Assets

The 22 required sprites are the original GIFs, still hosted by the Maeda Studio
site. Fetch them into `cpp/assets/ims/` with `tools/fetch-assets.sh`:

```
BASE="http://maedastudio.com/1997/cal3/ims"
# names: shiseido maeda1 maeda2 b0..b9 r0..r9
```

Known sizes (verification aid; the program must read sizes at runtime):

| Sprite | Size (W×H) | Use |
| --- | --- | --- |
| `shiseido.gif` | 107×22 | bottom-left logo |
| `maeda2.gif` | 15×58 | top-right signature |
| `maeda1.gif` | 81×23 | unused by the reference (may be ignored) |
| `b0..b9.gif` | ~10–14 × 14 | **month** digits (one width per digit) |
| `r0..r9.gif` | ~7–12 × 14 | **day** digits (one width per digit) |

Requirements:

- Locate assets **relative to the executable** as `<exe_dir>/assets/ims/`, and also
  accept a `FLORA_ASSETS` environment override. If an asset is missing, print a clear
  error to stderr and exit non-zero (do not crash, do not silently render nothing).
- Sprites are GIF 87a, 1-bit/grayscale on a **black opaque background**; alpha is not
  needed, but do not introduce color fringing.
- Decode GIF with the vendored `stb_image.h` (no SDL2_image dependency).
- `tools/fetch-assets.sh` must be idempotent and use `curl -fsSL`.

---

## 7. Rendering fidelity

This is a **translation**, not a redesign. The C++ output must match the Java
reference:

- Same 400×400 black frame, same petal count (13 drawn), same shapes, same
  rotation speed, same scaling from cursor distance, same color ramp to black,
  same logo/signature positions, same date layout, same hard (non-antialiased) edges.
- The three fixtures must visually match `reference/initial.png`,
  `reference/transition.png`, `reference/preset3.png`. Compare frame-by-frame
  (identical pixels are the goal; allow at most imperceptible rounding diffs and
  document any difference).
- Geometry traces (`.geometry`) must use the identical format and should match a
  Java-generated trace line for line for the same scenario.

---

## 8. Build and run

Primary platform: **macOS (Apple clang)**; keep it portable to Linux.

```sh
# one-time asset fetch
bash cpp/tools/fetch-assets.sh

# build
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build -j

# the required launchable binary
cp cpp/build/flora ./flora          # or a relative symlink

# run
./flora
./flora --smoke-test
./flora --fixtures cpp/scenarios/initial.txt /tmp/flora-out
```

`CMakeLists.txt` must: require C++17, find SDL2 via `find_package(SDL2)` or
`pkg-config`, link `SDL2` (+ `dl`/`m` as needed), enable `-O2`, and place the
`flora` executable in `cpp/build/`. Prefer warnings `-Wall -Wextra`. Do not require
network access at build or run time.

---

## 9. Acceptance criteria (must all pass)

1. **Builds clean** from a clean `cpp/` tree with the two `cmake` commands above,
   no missing dependencies beyond system SDL2.
2. **`./flora` opens a 400×400 window** titled as specified and animates without
   crash for at least 60 s; mouse move changes the flower scale; click advances the
   morph.
3. **`./flora --smoke-test` exits 0** after exactly 100 frames.
4. **`./flora --fixtures …` runs headless** and writes `<name>.png` +
   `<name>.geometry` per scenario row, with the exact trace format of §5.8.
5. **Fidelity:** fixture PNGs match `reference/*.png` (visually and, in practice,
   pixel-close).
6. **Asset handling:** a missing sprite produces a clear error and non-zero exit,
   never a crash.
7. **Binary present** at `code-translation-benchmark/flora` and runnable from a shell
   in a fresh terminal.
8. **README** documents build, run, fixtures, and asset fetch.
9. **No regressions to semantics:** presets, easing, transform stack, Bézier
   subdivision, petal color ramp and date layout all match §5.

---

## 10. Rules and constraints

- C++17, SDL2, vendored `stb_image` / `stb_image_write` only. No SDL2_image, no
  Qt/GTK/OpenGL, no JVM, no network at runtime, no browser.
- Keep the source readable, split by concern (`ctm`, `easing`, `panel`, `assets`,
  `flora_clock`, `main`), with brief comments for the non-obvious integer math.
- Preserve Java integer semantics: `int` vs `long` intermediates, integer division,
  truncation toward zero, and the `push`/`pop` quirks. Document any place where C++
  cannot match exactly.
- Deterministic fixtures: with a fixed `time` and `date`, repeated runs must produce
  byte-identical PNGs and traces.
- Do not stop at a design document or uncompiled source: the task is complete only
  when `flora` builds, runs, and reproduces the reference output.
- If the referenced `documentation/restoration-log.md` is expected, note its absence
  rather than inventing content.

---

## 11. Deliverable checklist

- [ ] `cpp/` project with CMake, sources, `third_party/` headers, `assets/ims/`,
      `scenarios/`, `tools/fetch-assets.sh`, `README.md`
- [ ] `cmake` build succeeds on a clean tree
- [ ] `flora` executable present and launchable from the shell
- [ ] `--smoke-test` exits 0 after 100 frames
- [ ] `--fixtures` produces PNGs + traces matching `reference/`
- [ ] All 9 acceptance criteria in §9 satisfied
- [ ] Short final report: artifacts, exact build/run commands, verification performed
      (report only checks actually run)
