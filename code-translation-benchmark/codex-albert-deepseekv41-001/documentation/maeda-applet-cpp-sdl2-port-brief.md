# John Maeda Java Applets → C++/SDL2 Desktop Port — Reusable Brief

**Document type:** implementation brief / specification for an autonomous coding
model (DeepSeek V4 class).
**Scope:** reusable across John Maeda’s restored Java applets of the same family
(Shiseido calendar / Maeda Studio applets).
**Target:** a turn-key, compilable, runnable **C++11 + SDL2** desktop application
that reproduces the rendering of a restored Java applet, launched as a shell binary.

> **How to use this brief.**
> 1. Copy this file next to the app’s sources (keep it under `documentation/`).
> 2. Fill in the **Parameter Sheet (§1)** for the target app, reading the actual
>    `java/` sources and `web-page/`.
> 3. Implement §2–§11 exactly. Where a default assumption does not match the source,
>    trust the **source** and record the deviation.
> 4. §12 (Acrobat-free checklist) and **Appendix B** (Flora worked example) are
>    references; Appendix A documents the recurring Maeda helper classes.

---

## 1. Parameter Sheet (fill per app)

Everything that varies from app to app is captured here. Values marked *(default)*
reflect the common Maeda applet pattern and must be confirmed against the sources.

| Field | Value |
| --- | --- |
| App title (window) | `{{WINDOW_TITLE}}` *(default: `<Author> — <App Name>`)* |
| Source directory | `java/` |
| Original web page | `web-page/{{PAGE_FILE}}` |
| Reference images | `reference/*.png` |
| Window / frame size | `{{W}}×{{H}}` px *(default 400×400)* |
| Frame timer interval | `{{TICK_MS}}` ms *(default 5)* |
| Clock class / override | `{{CLOCK_CLASS}}` (time in ms, optional fixed date) |
| Transform class | `{{CTM_CLASS}}` *(default `Ctm`: affine + fixed-point Bézier)* |
| Easing classes | `{{EASING_CLASSES}}` *(default `efloat`/`eint`)* |
| Panel / main drawing class | `{{PANEL_CLASS}}` *(default `cal?panel`)* |
| Entry point | `{{MAIN_CLASS}}` *(default `FloraReference`-style)* |
| Asset base URL | `{{ASSET_BASE_URL}}` *(default `http://maedastudio.com/<year>/<path>/ims`)* |
| Asset sprite list | `{{SPRITES}}` (names + roles; see §7) |
| CLI modes | `{{CLI_MODES}}` *(default: interactive / `--smoke-test` / `--fixtures`)* |
| Fixture scenario format | `{{FIXTURE_FORMAT}}` (see §4) |
| Morph / preset table | `{{PRESETS}}` (list; see §6.6) |
| Branding sprites & layout | `{{BRANDING}}` (logo, signature, positions) |
| Extra behaviors | `{{EXTRA}}` (e.g. geometry trace, sound, multi-panel) |
| Deliverable binary name | `flora` *(default: `{{BIN}}`)* |

---

## 2. Background / why this is a “repair”

The Java sources in `java/` are a **restoration**: they are reconstructed from
decompiled bytecode (the archives carry a header such as *“Adapted from the
archived CFR 0.152 reconstruction”*). They are a faithful reconstruction of an
applet that originally ran in a browser as `<applet code="…class" width="…"
height="…">`.

- The original context is preserved in `web-page/` (the Maeda Studio page, often
  including the interaction instructions in prose).
- The original raster assets (GIF sprites) are frequently **not** in the repository
  but are still hosted by the original site (see §7).
- The goal is a **desktop version** — no browser, no JVM — that looks and behaves
  the same, delivered ready to run.

**Golden rule:** this is a *translation*, not a redesign. Preserve the reconstructed
behavior, **including its idiosyncrasies**. Do not “fix” the math or “improve” the
rendering.

---

## 3. Deliverables and repository layout

Create the port inside the app’s benchmark folder:

```
<app-root>/
├── cpp/
│   ├── CMakeLists.txt
│   ├── src/                    # split by concern, see §5
│   ├── third_party/            # vendored stb_image.h, stb_image_write.h
│   ├── assets/ims/             # required sprites (§7), fetched via script
│   ├── scenarios/              # fixture scenario files (§4)
│   ├── tools/fetch-assets.sh   # downloads the sprites
│   └── README.md               # exact build & run instructions
├── documentation/
│   └── <this brief>.md
└── {{BIN}}                     # REQUIRED: launchable binary (§9)
```

**Required outputs**

1. A clean `cmake` build producing the executable (`{{BIN}}`).
2. The **launchable binary present at `<app-root>/{{BIN}}`** (copy/symlink), so
   `./{{BIN}}` works straight from the shell.
3. `cpp/assets/ims/` populated with the sprites, plus an idempotent
   `tools/fetch-assets.sh`.
4. `cpp/README.md` documenting build, run, and the fixture workflow.
5. Fixture outputs that match the images in `reference/`.

---

## 4. CLI contract (generic pattern)

Most Maeda applets expose three modes; confirm the entry point’s argument handling
in `{{MAIN_CLASS}}` and mirror it exactly.

| Invocation | Behavior |
| --- | --- |
| `./{{BIN}}` | Open the `{{W}}×{{H}}` window and run the animation loop indefinitely. |
| `./{{BIN}} --smoke-test` | Same, but close automatically after **100 rendered frames** and exit `0`. |
| `./{{BIN}} --fixtures SCENARIO OUTPUT` | Headless: no window; per scenario row write `OUTPUT/<name>.png` (+ `<name>.geometry` if the app traces geometry). |
| anything else | Print usage to stderr and exit non-zero. |

**Scenario file format** (whitespace-separated, `#` comments and blank lines
skipped). `{{FIXTURE_FORMAT}}` — Flora’s is the canonical example:

```
time_ms  mouse_x  mouse_y  click_count  month  day  name
```

Per row: set the clock to `time_ms` (it does **not** advance); set the cursor; apply
`click_count` simulated “click/morph” actions; if the app has a date, pin it (Flora
uses `2024-<month>-<day>`); if `name == "-"`, render one frame and discard it,
otherwise write `<name>.png` (+ `.geometry` while rendering if applicable).

Provide at least one scenario per `reference/*.png` under `cpp/scenarios/`.

---

## 5. Runtime architecture (SDL2)

- **Window:** `{{W}}×{{H}}`, non-resizable, title `{{WINDOW_TITLE}}`. Ensure it is
  foregrounded on macOS.
- **Software rendering (expected):** keep a 32-bit ARGB software framebuffer,
  exactly like the Java offscreen `BufferedImage`. Draw into it with your own
  **integer, non-antialiased** scanline polygon fill, then upload once per frame via
  `SDL_UpdateTexture` + `SDL_RenderCopy`. This preserves Java’s hard-edged
  `Graphics.fillPolygon` look and makes headless fixture output trivial.
  Do **not** use SDL anti-aliased primitives or GPU geometry for the shapes.
- **Frame loop:** the Java `javax.swing.Timer` fires every `{{TICK_MS}}` ms; render
  one frame per tick and pace the loop with `SDL_Delay` (do not spin hot).
- **Input:** update `(mx,my)` on mouse **move, drag, press**; on **release** invoke
  the app’s click handler (`{{EXTRA}}` may add more).
- **Headless fixtures:** `--fixtures` must never create a window.
- **Suggested source split:** `main` (CLI + loop), `clock`, `ctm` (or the app’s
  transform class), `easing`, `panel`, `assets`.

**Dependencies:** SDL2 only (via `pkg-config`/`find_package`). Vendor
`stb_image.h` (GIF-capable) and `stb_image_write.h` (PNG) so **no SDL2_image** is
required.

---

## 6. Behavioral porting rules

Port faithfully from the sources. These rules apply across the family; the concrete
numbers come from `java/` and go in §1 (see Appendix B for a filled example).

### 6.1 Clock (`{{CLOCK_CLASS}}`)
- Capture `epoch` (wall ms) and a monotonic `origin` (ns) once.
- `tick()`: `time = epoch + (now_ns - origin)/1e6`; `now()` returns `time`.
- `date()` returns a fixed date if set, else the real local date.
- `time` starts at 0 until the first `tick()`; in fixtures it is set and frozen.

### 6.2 Panel state
- Record counts/sizes (`npetals`, `nmorphs`, `gdur`, etc.) from the source.
- Run the app’s initializer (e.g. `doinits`) to register presets **in order**.

### 6.3 Per-frame composition
- Determine the exact draw order from the panel’s redraw method (background fill,
  logo/signature blits, overlays/date, transform push/translate/scale, shapes, pop,
  border). Reproduce coordinates literally.
- Compute any cursor-driven parameter (e.g. scale from distance to center, clamped)
  with the same formula and clamp.

### 6.4 Shapes / paths (`{{CTM_CLASS}}`)
- Preserve the affine transform, the transform **stack semantics** (including any
  off-by-one / double-copy quirks in `push`/`pop`), and the fixed-point Bézier
  subdivision (e.g. 16.16 with a flatness threshold) **literally**.
- Path points map through the matrix with the source’s rounding (`+0.5f`, int cast).
- Fills are integer, non-antialiased, Java `fillPolygon` semantics.

### 6.5 Easing (`{{EASING_CLASSES}}`)
- Keep the exact cubic formula, the `t == -1` first-call behavior, and the
  **type semantics**: float variants use float math, integer variants use integer
  division/truncation, with `long`/wide intermediates where the source uses them.
- `next()`, `rewind()`, `val()`, `donep` semantics must match.

### 6.6 Morph / presets (`{{PRESETS}}`)
- Reproduce the preset table verbatim (all fields + colors). Color constants follow
  `java.awt.Color` (e.g. `pink=(255,175,175)`, `orange=(255,200,0)`).
- Preserve the initializer quirk (e.g. the first preset may be pushed twice) and the
  cyclic advance on click (`morphc = (morphc+1) % nmorphs`).

### 6.7 Colors, date, branding
- Match palette and blend math exactly (per-channel `lerp` + clamp).
- If the app draws a date from digit sprites, reproduce the layout (positions, the
  `+1` advance between digits, separator line, sprite-specific widths).

### 6.8 Geometry trace (if present)
- If the source emits a trace (e.g. during fixtures), match the format byte-for-byte.
  Flora’s format is `rgb npts x0 y0 x1 y1 …` per filled path, with `rgb = color &
  0xFFFFFF`.

---

## 7. Assets

Maeda applets load small GIF sprites from an `ims/` directory (often under the
original site path). General pattern:

- Base URL: `{{ASSET_BASE_URL}}` (default
  `http://maedastudio.com/<year>/<path>/ims`).
- Sprites: `{{SPRITES}}` (logos, signatures, per-digit `0..9` sets, backgrounds…).
- Fetch them into `cpp/assets/ims/` with `tools/fetch-assets.sh`
  (`curl -fsSL`, idempotent). Reference the exact URLs in the script.
- Locate assets at runtime **relative to the executable** as
  `<exe_dir>/assets/ims/`, and accept an environment override (e.g. `FLORA_ASSETS`).
- If an asset is missing, print a clear error to stderr and exit non-zero — never
  crash and never silently render nothing.
- Decode GIF with the vendored `stb_image.h` (no SDL2_image). Sprites are typically
  small, 1-bit/grayscale on an opaque background; preserve sizes read at runtime and
  avoid color fringing.

---

## 8. Fidelity and verification

- The C++ output must match the Java reference: same frame size, same background,
  same shapes/counts, same animation rates, same layout, same hard (non-antialiased)
  edges, same colors.
- Fixture outputs must visually match `reference/*.png` (identical pixels are the
  goal; allow at most imperceptible rounding differences and **document** any
  difference).
- If the app traces geometry, `.geometry` output should match a Java-generated trace
  line-for-line for the same scenario.

---

## 9. Build and run

Primary platform: **macOS (Apple clang)**; keep portable to Linux.

```sh
# one-time asset fetch
bash cpp/tools/fetch-assets.sh

# build
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release
cmake --build cpp/build -j

# the required launchable binary
cp cpp/build/{{BIN}} ./{{BIN}}          # or a relative symlink

# run
./{{BIN}}
./{{BIN}} --smoke-test
./{{BIN}} --fixtures cpp/scenarios/{{SCENARIO}}.txt /tmp/{{BIN}}-out
```

`CMakeLists.txt` must require C++11 (`set(CMAKE_CXX_STANDARD 11)` /
`CXX_STANDARD_REQUIRED ON`), find SDL2 (`find_package` or `pkg-config`),
link SDL2 (+ `dl`/`m` as needed), enable `-O2`, use `-Wall -Wextra`, and place the
executable in `cpp/build/`. No network access at build or run time.

---

## 10. Acceptance criteria (generic)

1. **Builds clean** from a clean `cpp/` tree with the two `cmake` commands above;
   no dependency beyond system SDL2.
2. **`./{{BIN}}` opens the window** with the specified size/title and animates for
   ≥ 60 s without crashing; input behaves as the reference (e.g. move changes scale,
   click advances the morph).
3. **`./{{BIN}} --smoke-test` exits 0** after exactly 100 frames.
4. **`./{{BIN}} --fixtures …` runs headless** and writes the expected outputs per
   scenario row, with the exact trace format if applicable.
5. **Fidelity:** fixture outputs match `reference/*.png` (visually, pixel-close).
6. **Asset handling:** a missing sprite yields a clear error + non-zero exit.
7. **Binary present** at `<app-root>/{{BIN}}` and runnable from a fresh shell.
8. **README** documents build, run, fixtures, and asset fetch.
9. **No semantic regressions:** presets, easing, transform stack, Bézier
   subdivision, color ramp, and overlays match the source.

---

## 11. Rules and constraints

- C++11 (no C++14/17/20 features), SDL2, vendored `stb_image`/`stb_image_write`
  only. No SDL2_image, no
  Qt/GTK/OpenGL, no JVM, no browser, no network at runtime.
- Preserve Java integer semantics: `int` vs `long` intermediates, integer division,
  truncation toward zero, and transform-stack quirks. Document anything that cannot
  match exactly.
- Deterministic fixtures: with fixed clock/date, repeated runs produce byte-identical
  PNGs and traces.
- Keep the source readable, split by concern, with brief comments for non-obvious
  integer math.
- Do not stop at a design document or uncompiled source: the task is complete only
  when the binary builds, runs, and reproduces the reference output.
- If a referenced document (e.g. `documentation/restoration-log.md`) is absent, note
  its absence rather than inventing content.

---

## 12. Delivery checklist (copy per app)

- [ ] Parameter Sheet (§1) filled from the actual sources
- [ ] `cpp/` project with CMake, sources, vendored headers, `assets/ims/`,
      `scenarios/`, `tools/fetch-assets.sh`, `README.md`
- [ ] `cmake` build succeeds on a clean tree
- [ ] `{{BIN}}` present and launchable from the shell
- [ ] `--smoke-test` exits 0 after 100 frames
- [ ] `--fixtures` produces PNGs (+ traces) matching `reference/`
- [ ] All §10 acceptance criteria satisfied
- [ ] Short final report: artifacts, exact build/run commands, verification performed
      (report only checks actually run)

---

## Appendix A — recurring Maeda helper classes

These classes recur (sometimes renamed) across the family; port them faithfully.

- **Transform + path builder** (`Ctm`): `gctm[9]` affine matrix, `ctms[16][9]`
  stack, `push`/`pop`, matrix builders (translate/scale/rotate/skew), float→int
  mapping with `+0.5f`, and a recursive fixed-point Bézier subdivision
  (`BCurve`, 16.16 with a flatness threshold). Includes a `fill()` that optionally
  emits a geometry trace. Watch for intentional quirks (e.g. `push` copying the
  matrix twice; `pop` decrementing before the bounds check).
- **Easing** (`efloat` / `eint`): symmetric cubic ease over `dur` ms using
  `FloraClock.now()`; float vs integer variants; `t == -1` returns the start value
  and latches the start time; `next()` restarts from the current value.
- **Idle panel base** (`IdlePanel`): a Swing `JPanel` owning mouse position and an
  offscreen `BufferedImage`; `renderFrame()` draws into the buffer; `paintComponent`
  blits it. Maps naturally to an SDL surface + per-frame upload.
- **Clock** (`FloraClock`): monotonic ms clock; optional fixed date; optional
  geometry `PrintWriter`.
- **Reference harness** (`FloraReference`): CLI (`--smoke-test`, `--fixtures`),
  asset discovery relative to the app, and the fixture driver.

---

## Appendix B — Worked example: Flora (Calendar #3)

Fills the Parameter Sheet for `code-translation-benchmark/` (Maeda/Shiseido 1997
calendar applet).

| Field | Value |
| --- | --- |
| Window title | `John Maeda — Flora Calendar` |
| Window / frame | 400×400, non-resizable |
| Tick | 5 ms (≈200 Hz) |
| Entry point | `FloraReference` |
| Transform | `Ctm` |
| Easing | `efloat` (float) + `eint` (int), `gdur = 2000` ms |
| Panel | `cal2panel extends IdlePanel` |
| Clock | `FloraClock` (`time` ms, `fixedDate`, `geometry`) |
| Asset base URL | `http://maedastudio.com/1997/cal3/ims` |
| Binary | `flora` |

### B.1 Assets (22 GIFs; sizes for verification)

`shiseido.gif` 107×22 (logo bottom-left), `maeda2.gif` 15×58 (signature top-right),
`maeda1.gif` 81×23 (unused), `b0..b9.gif` ≈10–14×14 (month digits),
`r0..r9.gif` ≈7–12×14 (day digits). All GIF 87a, grayscale on black.

### B.2 Composition order (`redraw`)

1. Fill black; 2. blit `shiseido` at `(8, H-h-8)`; 3. blit `maeda2` at `(W-w-1, 0)`;
4. `drawdate`; 5. `push(); translate(W/2,H/2)`; 6. `sc = 0.1 + sqrt(dist)/W`, clamp
`≤ 0.6`, `scale(sc,sc)`; 7. `petal`; 8. `pop()`; 9. dark-gray border `(64,64,64)`
rect `(0,0)…(W-1,H-1)`.

### B.3 Date (`drawdate`)

`x=W/4, y=H/4`; month via `b*` (advance `x += width+1` per digit); `x += 8`; white
vertical line `(x-5, y-bimh/4)…(x-5, y+bimh+bimh/4)`; day via `r*`.

### B.4 Petals

`rotval = (now % 10000) * 360 / 10000` degrees; `col = eased RGB`; `n = 14`; loop
`i = 1..13` (13 petals drawn): `push(); rotate(i*360/n + rotval);
setColor(blend(col, black, i/(n-1))); petal2(a,b,c,d,e,f,g,h); pop()`.
`petal2(loarm,arma,armb,rot,x,y,hiarm,hiy)` uses radians and `double` trig; four
`curveto` segments as in the source. `blend` = per-channel `lerp` + clamp.
Quirk: `ca..ch` are only read on `i==1`, then reused.

### B.5 Morph presets (`doinits`, 8 entries)

| # | a | b | c | d | e | f | g | h | color |
| - | - | - | - | - | - | - | - | - | - |
| 1 | 15.5242 | 208.079 | 48.3322 | 2.13347 | 84 | 295 | 13.0 | 257 | pink (255,175,175) |
| 2 | 35.3553 | 76.0592 | 94.6256 | 2.52134 | 111 | 202 | 80.1561 | 320 | (104,41,94) |
| 3 | 30.2655 | 72.2772 | 125.575 | -3.84251 | 104 | 305 | 58.0086 | 171 | blue (0,0,255) |
| 4 | 43.0 | 83.1865 | 107.517 | 0.321751 | 61 | 116 | 140.688 | 310 | orange (255,200,0) |
| 5 | 12.0416 | 160.178 | 64.2806 | 2.64723 | 110 | 244 | 9.21954 | 218 | red (255,0,0) |
| 6 | 90.7965 | 152.856 | 15.8114 | 0.566227 | 164 | 249 | 114.983 | 332 | white (255,255,255) |
| 7 | 30.0666 | 260.864 | 60.1664 | 0.855933 | 144 | 288 | 15.2643 | 147 | green (0,255,0) |
| 8 | 20.6155 | 50.04 | 54.0833 | 1.5153 | 58 | 139 | 47.8853 | 348 | yellow (255,255,0) |

`addval` calls `mvector` twice with preset 1 on first allocation; `morph()` advances
`morphc` cyclically; `mvector` eases all fields **and** the three color channels.

### B.6 Easing formula (`efloat`/`eint`)

```
if (t == -1) { t = now(); return a; }
dt = now() - t;
if (dt >= dur) { donep = true; cx = b; return b; }
dt = dur - dt;
cx = dt*dt*dt * (a-b) / (dur*dur*dur) + b;   // float math; eint truncates
```

### B.7 Geometry trace

Per filled path: `rgb npts x0 y0 x1 y1 …` with `rgb = color & 0xFFFFFF`, one line
per path (only while a trace writer is active).

### B.8 Reference fixtures

`reference/initial.png`, `reference/transition.png`, `reference/preset3.png` →
`cpp/scenarios/{initial,transition,preset3}.txt` using the §4 scenario format with
`fixedDate = 2024-<month>-<day>`.
